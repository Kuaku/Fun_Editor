#include "file_explorer.h"
#include "string_input.h"
#include "../../editor/editor.h"
#include "../../utils/utf8.h"

void InitOpenPathSet(OpenPathSet* set) {
    set->paths = NULL;
    set->count = 0;
    set->capacity = 0;
}

void ClearOpenPathSet(OpenPathSet* set) {
    for (size_t i = 0; i < set->count; i++) {
        free(set->paths[i]);
    }

    free(set->paths);
    set->paths = NULL;
    set->count = 0;
    set->capacity = 0;
}

size_t OpenPathSetFind(const OpenPathSet* set, const char* path) {
    if (set->count == 0) return 0;

    size_t lo = 0;
    size_t hi = set->count;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(set->paths[mid], path);
        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

bool OpenPathSetContains(const OpenPathSet* set, const char* path) {
    size_t pos = OpenPathSetFind(set, path);
    return pos < set->count && strcmp(set->paths[pos], path) == 0;
}

void OpenPathSetInsert(OpenPathSet* set, const char* path) {
    size_t pos = OpenPathSetFind(set, path);

    if (pos < set->count && strcmp(set->paths[pos], path) == 0) {
        return;
    }

    if (set->count >= set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 32;
        set->paths = realloc(set->paths, set->capacity * sizeof(char*));
    }

    if (pos < set->count) {
        memmove(&set->paths[pos + 1], &set->paths[pos], (set->count - pos) * sizeof(char*));
    }

    set->paths[pos] = strdup(path);
    set->count++;
}

void OpenPathSetRemove(OpenPathSet* set, const char* path) {
    size_t pos = OpenPathSetFind(set, path);

    if (pos >= set->count || strcmp(set->paths[pos], path) != 0) {
        return;
    }

    free(set->paths[pos]);
    if (pos + 1 < set->count) {
        memmove(&set->paths[pos], &set->paths[pos + 1],
                (set->count - pos - 1) * sizeof(char*));
    }
    set->count--;
}

void OpenPathSetToggle(OpenPathSet* set, const char* path) {
    if (OpenPathSetContains(set, path)) {
        OpenPathSetRemove(set, path);
    } else {
        OpenPathSetInsert(set, path);
    }
}

void FlatEntryListReset(FlatEntryList* list) {
    list->count = 0;
}

void FlatEntryListClear(FlatEntryList* list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void FlatEntryListPush(FlatEntryList* list, CacheHandle handle, int depth, bool is_dir) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 256;
        list->items = realloc(list->items, list->capacity * sizeof(FlatEntry));
    }
    list->items[list->count].handle = handle;
    list->items[list->count].depth = depth;
    list->items[list->count].is_dir = is_dir;
    list->count++;
}

bool StrContainsCaseInsensitive(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack) return false;
    size_t h_len = strlen(haystack);
    size_t n_len = strlen(needle);
    if (n_len > h_len) return false;
    for (size_t i = 0; i <= h_len - n_len; i++) {
        bool match = true;
        for (size_t j = 0; j < n_len; j++) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

typedef struct {
    CacheHandle handle;
    int depth;
} WalkFrame;

void RebuildAllEntries(FileSystem* system, FlatEntryList* all) {
    FlatEntryListReset(all);

    size_t stack_cap = 64;
    size_t stack_count = 0;
    WalkFrame* stack = malloc(stack_cap * sizeof(WalkFrame));

    for (int i = (int)system->root_children.count - 1; i >= 0; i--) {
        if (stack_count >= stack_cap) {
            stack_cap *= 2;
            stack = realloc(stack, stack_cap * sizeof(WalkFrame));
        }

        stack[stack_count++] = (WalkFrame){ system->root_children.items[i], 0 };
    }

    while (stack_count > 0) {
        WalkFrame frame = stack[--stack_count];
        FileCacheEntry* entry = FileSystemGetEntry(system, frame.handle);
        if (!entry) continue;

        bool is_dir = entry->type == TYPE_DIR;
        FlatEntryListPush(all, frame.handle, frame.depth, is_dir);

        if (is_dir) {
            for (int i = (int)entry->children.count - 1; i >= 0; i--) {
                if (stack_count >= stack_cap) {
                    stack_cap *= 2;
                    stack = realloc(stack, stack_cap * sizeof(WalkFrame));
                }
                stack[stack_count++] = (WalkFrame){ entry->children.items[i], frame.depth + 1};
            }
        }
    }

    free(stack);
}

void RebuildVisibleEntries(FileSystem* system, FlatEntryList* all, FlatEntryList* visible, const OpenPathSet* open_dirs, const char* search, size_t search_len) {
    FlatEntryListReset(visible);
    if (search_len == 0) {
        int skip_below = INT_MAX;

        for (size_t i = 0; i < all->count; i++) {
            FlatEntry* fe = &all->items[i];

            if (fe->depth > skip_below) continue;

            skip_below = INT_MAX;

            FileCacheEntry* entry = FileSystemGetEntry(system, fe->handle);
            if (!entry) continue;

            FlatEntryListPush(visible, fe->handle, fe->depth, fe->is_dir);

            if (fe->is_dir && !OpenPathSetContains(open_dirs, entry->rel_path)) {
                skip_below = fe->depth;
            }
        }
    } else {
        char search_lower[256];
        StrToLower(search, search_lower, sizeof(search_lower));

        for (size_t i = 0; i < all->count; i++) {
            FlatEntry* fe = &all->items[i];
            FileCacheEntry* entry = FileSystemGetEntry(system, fe->handle);
            if (!entry) continue;

            if (strstr(entry->name_lower, search_lower)) {
                FlatEntryListPush(visible, fe->handle, 0, fe->is_dir);
            }
        }
    }
}

void EnsurePathVisible(FileSystem* system, OpenPathSet* open_dirs, const char* target_rel_path) {
    char path_copy[PATH_MAX_LEN];
    strncpy(path_copy, target_rel_path, PATH_MAX_LEN - 1);
    path_copy[PATH_MAX_LEN - 1] = '\0';

    char* last_sep;

    while (1) {
        last_sep = strrchr(path_copy, '/');
        #ifdef _WIN32
        char* last_bslash = strrchr(path_copy, '\\');
        if (last_bslash && (!last_sep || last_bslash > last_sep)) {
            last_sep = last_bslash;
        }
        #endif

        if (!last_sep) break;

        *last_sep = '\0';
        if (path_copy[0] != '\0') {
            OpenPathSetInsert(open_dirs, path_copy);
        }
    }
}

void FileExplorerSearchInsertChar(FileExplorerState* state, uint32_t codepoint) {
    char utf8_buffer[4];
    size_t utf8_length = utf8_encode(codepoint, utf8_buffer);

    if (state->search.length >= sizeof(state->search.buffer) - utf8_length) return;

    size_t cursor_byte = utf8_codepoint_to_offset(state->search.buffer, state->search.cursor);
    memmove(&state->search.buffer[cursor_byte + utf8_length],
            &state->search.buffer[cursor_byte],
            state->search.length - cursor_byte);
    memcpy(&state->search.buffer[cursor_byte], utf8_buffer, utf8_length);
    state->search.cursor++;
    state->search.length += utf8_length;
    state->search.buffer[state->search.length] = '\0';

    state->needs_rebuild_visible = true;
}

void FileExplorerSearchBackspace(FileExplorerState* state) {
    if (state->search.cursor == 0) return;

    size_t cursor_byte = utf8_codepoint_to_offset(state->search.buffer, state->search.cursor);
    size_t utf8_length = utf8_prev(state->search.buffer, state->search.buffer + cursor_byte);
    memmove(&state->search.buffer[cursor_byte - utf8_length],
            &state->search.buffer[cursor_byte],
            state->search.length - cursor_byte);
    state->search.cursor--;
    state->search.length -= utf8_length;
    state->search.buffer[state->search.length] = '\0';

    state->needs_rebuild_visible = true;
}

void FileExplorerSearchClear(FileExplorerState* state) {
    state->search.buffer[0] = '\0';
    state->search.length = 0;
    state->search.cursor = 0;
    state->needs_rebuild_visible = true;
}

void FileExplorerRender(Editor* editor, RenderNode* self) {
    Modal* modal = (Modal*)self->user_data;
    FileExplorerState* state = (FileExplorerState*)modal->state;
    FileSystem* system = &editor->file_system;
    Font font = editor->settings.editor_font;
    int font_size = editor->settings.font_size;
    int row_height = font_size + modal->style.widget_spacing;
    int pad = modal->style.content_padding.x;
    int search_bar_height = font_size + 12;

    PushScissor(&editor->render_system.render_queue, self->inner_bounds);

    int search_y = self->inner_bounds.position.y + pad;
    PushRect(&editor->render_system.render_queue, (Rect){{self->inner_bounds.position.x + pad, search_y}, {self->inner_bounds.size.x - pad * 2, search_bar_height}}, (RenderColor){modal->style.input_background.r, modal->style.input_background.g, modal->style.input_background.b, modal->style.input_background.a});

    const char* search_prefix = "Search: ";
    PushText(&editor->render_system.render_queue, search_prefix, (Position){self->inner_bounds.position.x + pad + 4, search_y + 6}, font_size * 0.8, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});

    Position prefix_size = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, search_prefix, font_size * 0.8, 1);
    PushText(&editor->render_system.render_queue, state->search.buffer, (Position){self->inner_bounds.position.x + pad + 4 + prefix_size.x, search_y + 6}, font_size * 0.8, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});

    int list_y = search_y + search_bar_height + pad;
    int list_height = self->inner_bounds.size.y - (list_y - self->inner_bounds.position.y) - pad;
    size_t visible_rows = list_height / row_height;

    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    if (state->selected_index >= state->scroll_offset + visible_rows) {
        state->scroll_offset = state->selected_index - visible_rows + 1;
    }

    for (size_t i = state->scroll_offset; i < state->visible.count; i++) {
        size_t display_i = i - state->scroll_offset;
        if (display_i >= visible_rows) break;

        FlatEntry* entry = &state->visible.items[i];
        FileCacheEntry* file_entry = FileSystemGetEntry(system, entry->handle);
        if (!file_entry) continue;

        int y = list_y + display_i * row_height;

        if (i == state->selected_index) {
            PushRect(&editor->render_system.render_queue, (Rect){{self->inner_bounds.position.x + pad, y - 2}, {self->inner_bounds.size.x - pad * 2, row_height}}, (RenderColor){modal->style.selection.r, modal->style.selection.g, modal->style.selection.b, modal->style.selection.a});
        }

        int indent = entry->depth * 16;

        const char* icon = entry->is_dir ?
            (OpenPathSetContains(&state->open_dirs, file_entry->rel_path) ? "▼ " : "▶ ") :
            "  ";

        Color text_color = entry->is_dir ? modal->style.focused_border : modal->style.text;

        char display_text[NAME_MAX_LEN + 4];
        snprintf(display_text, sizeof(display_text), "%s%s", icon, file_entry->name);

        PushText(&editor->render_system.render_queue, display_text, (Position){self->inner_bounds.position.x + pad + indent, y + modal->style.widget_spacing / 2}, font_size, 1, (RenderColor){text_color.r, text_color.g, text_color.b, text_color.a});
    }

    PushScissorPop(&editor->render_system.render_queue);
}

void FileExplorerUpdate(Modal* modal) {
    FileExplorerState* state = (FileExplorerState*)modal->state;
    Editor* editor = state->editor;
    FileSystem* system = &editor->file_system;

    if (!system->poll_state.in_progress && system->dirty) {
        state->needs_rebuild_all = true;
    }

    if (state->needs_rebuild_all) {
        RebuildAllEntries(system, &state->all);
        state->needs_rebuild_all = false;
        state->needs_rebuild_visible = true;
    }

    if (state->needs_rebuild_visible) {
        CacheHandle selected_handle = CACHE_HANDLE_INVALID;
        const char* selected_path = NULL;

        if (state->selected_index < state->visible.count) {
            selected_handle = state->visible.items[state->selected_index].handle;
            FileCacheEntry* entry = FileSystemGetEntry(system, selected_handle);
            if (entry) {
                selected_path = entry->rel_path;
            }
        }

        if (state->search.length == 0 && selected_path && selected_path[0] != '\0') {
            EnsurePathVisible(system, &state->open_dirs, selected_path);
        }

        RebuildVisibleEntries(system, &state->all, &state->visible, &state->open_dirs, state->search.buffer, state->search.length);
        state->needs_rebuild_visible = false;

        if (!cache_handle_is_invalid(selected_handle)) {
            bool found = false;
            for (size_t i = 0; i < state->visible.count; i++) {
                if (cache_handle_eq(state->visible.items[i].handle, selected_handle)) {
                    state->selected_index = i;
                    found = true;
                    break;
                }
            }

            if (!found && state->visible.count > 0) {
                if (state->selected_index >= state->visible.count) {
                    state->selected_index = state->visible.count - 1;
                }
            }
        }

        if (state->visible.count == 0) {
            state->selected_index = 0;
            state->scroll_offset = 0;
        } else {
            if (state->selected_index >= state->visible.count) {
                state->selected_index = state->visible.count - 1;
            }
            if (state->scroll_offset > state->selected_index) {
                state->scroll_offset = state->selected_index;
            }
        }
    }
}

void OnCreateFileOrDirConfirmed(Modal* modal, bool confirmed, void* result, void* user_data) {
    CreateFileContext* ctx = (CreateFileContext*)user_data;
    if (!confirmed) {
        free(ctx);
        return;
    }

    const char* input = (const char*)result;

    CreatePathUnderRoot(ctx->base_path, input);
    if (strlen(ctx->editor->file_system.root_path) > 0) {
        ctx->editor->file_system.poll_state.request_validation = true;
    }
    free(result);
    free(ctx);
}

void FileExplorerInput(Modal* modal, RawInput input) {
    FileExplorerState* state = (FileExplorerState*)modal->state;
    Editor* editor = state->editor;
    FileSystem* system = &editor->file_system;

    switch (input.key) {
        case KEY_N: {
            if (state->search.length == 0 && HasModifiers(input.modifiers, MODI_CTRL)) {
                CreateFileContext* ctx = malloc(sizeof(CreateFileContext));
                ctx->editor = editor;
                strncpy(ctx->base_path, system->root_path, PATH_MAX_LEN - 1);
                ctx->base_path[PATH_MAX_LEN - 1] = '\0';

                if (state->visible.count > 0 && state->selected_index < state->visible.count) {
                    FlatEntry* sel = &state->visible.items[state->selected_index];
                    FileCacheEntry* sel_entry = FileSystemGetEntry(system, sel->handle);
                    if (sel_entry) {
                        char dir_rel[PATH_MAX_LEN];
                        if (sel->is_dir) {
                            strncpy(dir_rel, sel_entry->rel_path, PATH_MAX_LEN - 1);
                            dir_rel[PATH_MAX_LEN - 1] = '\0';
                        } else {
                            strncpy(dir_rel, sel_entry->rel_path, PATH_MAX_LEN - 1);
                            dir_rel[PATH_MAX_LEN - 1] = '\0';
                            char* last_sep = strrchr(dir_rel, '/');
                            #ifdef _WIN32
                            char* last_bslash = strrchr(dir_rel, '\\');
                            if (last_bslash && (!last_sep || last_bslash > last_sep))
                                last_sep = last_bslash;
                            #endif
                            if (last_sep) *last_sep = '\0';
                            else dir_rel[0] = '\0';
                        }
                        if (dir_rel[0] != '\0') {
                            PlatformJoinPath(ctx->base_path, PATH_MAX_LEN, system->root_path, dir_rel);
                        }
                    }
                }
                char display_prefix[PATH_MAX_LEN];
                snprintf(display_prefix, sizeof(display_prefix), "%s%c",
                        ctx->base_path, FILE_SYSTEM_SEPERATER);
                PushStringInputModal(editor, "Create File/Dir",
                                    OnCreateFileOrDirConfirmed, ctx, display_prefix);
            }
            return;
        }
        case KEY_UP: {
            if (state->selected_index > 0) {
                state->selected_index--;
            }
            return;
        }
        case KEY_DOWN: {
            if (state->selected_index + 1 < state->visible.count) {
                state->selected_index++;
            }
            return;
        }
        case KEY_ENTER: {
            if (state->visible.count == 0) return;

            FlatEntry* entry = &state->visible.items[state->selected_index];
            FileCacheEntry* file_entry = FileSystemGetEntry(system, entry->handle);
            if (!file_entry) return;

            if (entry->is_dir) {
                OpenPathSetToggle(&state->open_dirs, file_entry->rel_path);
                state->needs_rebuild_visible = true;
            } else {
                char full_path[PATH_MAX_LEN];
                snprintf(full_path, sizeof(full_path), "%s%c%s",
                        system->root_path,
                        #ifdef _WIN32
                        '\\',
                        #else
                        '/',
                        #endif
                        file_entry->rel_path);
                modal->result_data = strdup(full_path);
                CloseModal(&editor->modal_system, true);
            }
            return;
        }
        case KEY_ESCAPE: {
            CloseModal(&editor->modal_system, false);
            return;
        }
        case KEY_BACKSPACE: {
            FileExplorerSearchBackspace(state);
            return;
        }
    }

    if (input.is_char && input.key >= 32) {
        FileExplorerSearchInsertChar(state, input.key);
        return;
    }
}

void FileExplorerResult(Modal* modal, bool confirmed, void* result, void* user_data) {
    if (!confirmed) return;

    Editor* editor = (Editor*)user_data;
    const char* path = (const char*)result;
    if (path) {
        OpenOrSwitchToFile(editor, path);
    }
}

void FileExplorerCleanup(void* raw_state) {
    FileExplorerState* state = (FileExplorerState*)raw_state;

    ClearOpenPathSet(&state->open_dirs);
    FlatEntryListClear(&state->all);
    FlatEntryListClear(&state->visible);

    free(state);
}

void RegisterFileExplorerModal(Editor* editor) {
    FileExplorerState* state = calloc(1, sizeof(FileExplorerState));
    state->editor = editor;
    state->needs_rebuild_all = true;

    Modal* modal = CreateModal(
        &editor->modal_system,
        "File Explorer",
        (Position){600, 500},
        FileExplorerRender,
        FileExplorerUpdate,
        FileExplorerInput,
        FileExplorerCleanup,
        NULL,
        state
    );
    modal->style.draw_title = true;
    modal->style.title_padding = (Position){10, 10};
    modal->on_result = FileExplorerResult;
    modal->on_result_user_data = editor;
    modal->is_cached = true;
    modal->margin = (Position){50, 50};

    ModalAddLayout(modal, ApplyWantedSize);
    ModalAddLayout(modal, ApplyMinSize);
    ModalAddLayout(modal, ApplyMaxSize);
    ModalAddLayout(modal, ApplyMargin);
    ModalAddLayout(modal, CenterModal);

    RegisterModalToQuickCatch(&editor->modal_system, "file_explorer", modal);
}

void OpenFileExplorerModal(Editor* editor) {
    PushModalFromCache(&editor->modal_system, "file_explorer");

    Modal* top = GetTopModal(&editor->modal_system);
    if (top) {
        FileExplorerState* state = (FileExplorerState*)top->state;
    }
}
