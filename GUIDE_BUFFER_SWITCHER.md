# Buffer Switcher Modal — Implementation Guide

## Progress

### Done
- [x] Modal struct with `is_cached` flag
- [x] ModalSystem with stack + cache (init, create, push, pop, close, clear)
- [x] `RegisterModalToQuickCatch` / `PushModalFromCache`
- [x] `CloseModal` respects `is_cached`
- [x] `ClearModalSystem` cleans both stack and cache
- [x] `ModalSystemRender` runs layout functions, draws border/bg/title/content
- [x] `RawInput` struct + `InputSystemPollRawInput` + `HasModifiers`
- [x] `ModalInputFunc` uses `RawInput`
- [x] `ModalSystem` added to `Editor`, initialized in `CreateEditor`
- [x] `ClearModalSystem` called in `ClearEditor`

### Remaining Bugs

**1. `InitModalSystem()` line 327 — assigns calloc pointer to size_t**
```c
// BROKEN (line 327):
system.cache_capacity = calloc(system.stack_capacity, sizeof(Modal*));
// FIX: just delete this line — line 326 already sets cache_capacity,
// and line 328 already allocates modal_cache correctly.
```

**2. `RegisterModalToQuickCatch()` line 358 — typo**
```c
// BROKEN:
system->modal_keys[index] = strdub(key);
// FIX:
system->modal_keys[index] = strdup(key);
```

**3. `InputSystemPollRawInput()` — only captures characters**
Currently only calls `GetCharPressed()`. Arrow keys, Escape, Enter etc. are
never returned. See Step 2 below.

---

## Remaining Steps

### Step 1: Layout Pipeline Functions

The layout pipeline replaces hardcoded bounds computation. Each function is
a `LayoutFunc` that modifies `modal->bounds` in place. Order = priority.

#### 1a. Helper to add layouts to a modal

```c
void ModalAddLayout(Modal* modal, LayoutFunc layout) {
    if (modal->layouts == NULL) {
        modal->layout_capacity = 4;
        modal->layouts = calloc(modal->layout_capacity, sizeof(LayoutFunc));
    }
    if (modal->layout_count >= modal->layout_capacity) {
        modal->layout_capacity *= 2;
        modal->layouts = realloc(modal->layouts, modal->layout_capacity * sizeof(LayoutFunc));
    }
    modal->layouts[modal->layout_count++] = layout;
}
```

#### 1b. The five layout functions

```c
// Sets bounds.size to wanted_size. Typically the first in the pipeline.
void ApplyWantedSize(Modal* modal) {
    modal->bounds.size = modal->wanted_size;
}

// Clamps bounds.size so it doesn't exceed max_size.
// Only clamps axes where max_size > 0.
void ApplyMaxSize(Modal* modal) {
    if (modal->max_size.x > 0 && modal->bounds.size.x > modal->max_size.x) {
        modal->bounds.size.x = modal->max_size.x;
    }
    if (modal->max_size.y > 0 && modal->bounds.size.y > modal->max_size.y) {
        modal->bounds.size.y = modal->max_size.y;
    }
}

// Clamps bounds.size so it's at least min_size.
// Only clamps axes where min_size > 0.
void ApplyMinSize(Modal* modal) {
    if (modal->min_size.x > 0 && modal->bounds.size.x < modal->min_size.x) {
        modal->bounds.size.x = modal->min_size.x;
    }
    if (modal->min_size.y > 0 && modal->bounds.size.y < modal->min_size.y) {
        modal->bounds.size.y = modal->min_size.y;
    }
}

// Shrinks the modal if it + margin would exceed the screen.
// This can override min_size if it comes after ApplyMinSize in the pipeline.
void ApplyMargin(Modal* modal) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    size_t max_w = sw - modal->margin.x * 2;
    size_t max_h = sh - modal->margin.y * 2;
    if (modal->bounds.size.x > max_w) {
        modal->bounds.size.x = max_w;
    }
    if (modal->bounds.size.y > max_h) {
        modal->bounds.size.y = max_h;
    }
}

// Centers the modal on screen. Typically the last in the pipeline.
void CenterModal(Modal* modal) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    modal->bounds.position.x = (sw - modal->bounds.size.x) / 2;
    modal->bounds.position.y = (sh - modal->bounds.size.y) / 2;
}
```

#### 1c. Example pipelines

```c
// Standard modal: wanted size, clamped, centered with margin guarantee
ModalAddLayout(modal, ApplyWantedSize);
ModalAddLayout(modal, ApplyMinSize);
ModalAddLayout(modal, ApplyMaxSize);
ModalAddLayout(modal, ApplyMargin);    // margin wins over min_size
ModalAddLayout(modal, CenterModal);

// Alternative: min_size guaranteed even if margin overflows
ModalAddLayout(modal, ApplyWantedSize);
ModalAddLayout(modal, ApplyMaxSize);
ModalAddLayout(modal, ApplyMargin);
ModalAddLayout(modal, ApplyMinSize);   // min_size wins over margin
ModalAddLayout(modal, CenterModal);
```

#### 1d. Update `ModalSystemRender`

The render function should NOT compute bounds itself — the layout pipeline
does that. The current code already calls the layouts (line 438-440), so
just make sure there's no hardcoded bounds computation. The current render
function is correct in this regard — it runs layouts, then uses
`active_modal->bounds`.

#### 1e. Free layouts in cleanup

Add to `ClearModal` and the non-cached path in `CloseModal`:
```c
if (modal->layouts) {
    free(modal->layouts);
}
```

And in `ClearModalSystem` for cached modals too.

---

### Step 2: Fix `InputSystemPollRawInput`

```c
RawInput InputSystemPollRawInput() {
    RawInput input = {0};
    input.modifiers = GetCurrentModifiers();

    // Character input first (for typing)
    if (!HasModifiers(input.modifiers, MODI_CTRL | MODI_ALT | MODI_SUPER)) {
        int ch = GetCharPressed();
        if (ch != 0) {
            input.key = ch;
            return input;
        }
    }

    // Then key press (arrows, escape, enter, etc.)
    input.key = GetKeyPressed();
    return input;
}
```

---

### Step 3: Route modal input in `EditorHandleInput`

When a modal is active, send input to it instead of the editor:

```c
void EditorHandleInput(Editor* editor) {
    if (ModalSystemHasActive(&editor->modal_system)) {
        RawInput input = InputSystemPollRawInput();
        while (input.key != 0) {
            Modal* top = GetTopModal(&editor->modal_system);
            if (top && top->custom_input) {
                top->custom_input(top, input);
            }
            input = InputSystemPollRawInput();
        }
        return;
    }

    // Normal editor input (existing code unchanged)
    Action action = InputSystemPoll(&editor->input_system);
    while (action.type != ACTION_NONE) {
        if (editor->input_system.current_mode == MODE_TEXT) {
            DispatchInputTextMode(editor, action);
        } else if (editor->input_system.current_mode == MODE_COMMAND) {
            DispatchInputCommandMode(editor, action);
        }
        ClearAction(&action);
        action = InputSystemPoll(&editor->input_system);
    }
}
```

---

### Step 4: Render modals in `EditorRender`

```c
void EditorRender(Editor* editor) {
    ClearBackground(editor->settings.scheme.background_color);
    EditorRenderMode(editor);
    EditorRenderTextField(editor, GetEditorTextFieldSize(editor));
    EditorRenderCommand(editor);
    ModalSystemRender(editor);      // add this last
}
```

---

### Step 5: Buffer List modal — state + callbacks

#### 5a. State

```c
typedef struct {
    Editor* editor;
    size_t selected_index;
    size_t scroll_offset;
} BufferListState;
```

#### 5b. Render callback

```c
void BufferListRender(Modal* modal, Rect content) {
    BufferListState* state = (BufferListState*)modal->state;
    Editor* editor = state->editor;
    EditorState* es = &editor->state;
    Font font = editor->settings.editor_font;
    int font_size = editor->settings.font_size;
    int row_height = font_size + modal->style.widget_spacing;
    size_t visible_rows = content.size.y / row_height;

    // Auto-scroll to keep selection visible
    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    if (state->selected_index >= state->scroll_offset + visible_rows) {
        state->scroll_offset = state->selected_index - visible_rows + 1;
    }

    BeginScissorMode(content.position.x, content.position.y,
                     content.size.x, content.size.y);

    for (size_t i = state->scroll_offset; i < es->text_buffers_count; i++) {
        size_t display_i = i - state->scroll_offset;
        if (display_i >= visible_rows) break;

        int y = content.position.y + display_i * row_height;

        if (i == state->selected_index) {
            DrawRectangle(content.position.x, y,
                          content.size.x, row_height,
                          modal->style.selection);
        }

        const char* label = es->text_buffers[i].file_path
                          ? es->text_buffers[i].file_path
                          : "[untitled]";

        Color text_color = modal->style.text;
        if ((int)i == es->open_text_buffer_index) {
            text_color = modal->style.focused_border;
        }

        DrawTextEx(font, label,
                   (Vector2){content.position.x + 4,
                             y + modal->style.widget_spacing / 2},
                   font_size, 1, text_color);
    }

    EndScissorMode();
}
```

#### 5c. Input callback

The modal handles its own close/confirm via `CloseModal`:

```c
void BufferListInput(Modal* modal, RawInput input) {
    BufferListState* state = (BufferListState*)modal->state;
    size_t count = state->editor->state.text_buffers_count;

    switch (input.key) {
        case KEY_DOWN:
            if (state->selected_index + 1 < count) {
                state->selected_index++;
            }
            break;
        case KEY_UP:
            if (state->selected_index > 0) {
                state->selected_index--;
            }
            break;
        case KEY_ENTER:
            modal->result_data = (void*)(uintptr_t)state->selected_index;
            CloseModal(&state->editor->modal_system, true);
            break;
        case KEY_ESCAPE:
            CloseModal(&state->editor->modal_system, false);
            break;
    }
}
```

#### 5d. Result callback

```c
void BufferListResult(Modal* modal, bool confirmed, void* result, void* user_data) {
    if (!confirmed) return;

    Editor* editor = (Editor*)user_data;
    size_t index = (size_t)(uintptr_t)result;

    if (index < editor->state.text_buffers_count) {
        editor->state.open_text_buffer_index = (int)index;
    }
}
```

#### 5e. Cleanup

```c
void BufferListCleanup(void* state) {
    free(state);
}
```

---

### Step 6: Register + open the buffer list modal

#### 6a. Register at startup (uses cache)

```c
void RegisterBufferListModal(Editor* editor) {
    BufferListState* state = calloc(1, sizeof(BufferListState));
    state->editor = editor;
    state->selected_index = 0;
    state->scroll_offset = 0;

    Modal* modal = CreateModal(
        &editor->modal_system,
        "Open Buffers",
        (Position){500, 400},
        BufferListRender,
        BufferListInput,
        BufferListCleanup,
        state
    );
    modal->style.draw_title = true;
    modal->on_result = BufferListResult;
    modal->on_result_user_data = editor;
    modal->is_cached = true;
    modal->margin = (Position){50, 50};

    // Layout pipeline: wanted size, clamped, margin-safe, centered
    ModalAddLayout(modal, ApplyWantedSize);
    ModalAddLayout(modal, ApplyMinSize);
    ModalAddLayout(modal, ApplyMaxSize);
    ModalAddLayout(modal, ApplyMargin);
    ModalAddLayout(modal, CenterModal);

    RegisterModalToQuickCatch(&editor->modal_system, "buffer_list", modal);
}
```

Call in `main()` after `CreateEditor`:
```c
RegisterBufferListModal(&editor);
```

#### 6b. Open the modal

```c
void OpenBufferListModal(Editor* editor) {
    PushModalFromCache(&editor->modal_system, "buffer_list");

    Modal* top = GetTopModal(&editor->modal_system);
    if (top) {
        BufferListState* state = (BufferListState*)top->state;
        state->selected_index = editor->state.open_text_buffer_index >= 0
                              ? (size_t)editor->state.open_text_buffer_index : 0;
        state->scroll_offset = 0;
    }
}
```

---

### Step 7: Key binding + action

```c
// In ActionType enum (before ACTION_EXECUTE_COMMAND):
ACTION_OPEN_BUFFER_LIST,

// In default_normal_bindings[]:
{ KEY_B, MODI_CTRL, ACTION_OPEN_BUFFER_LIST },

// In ActionTypeToString():
case ACTION_OPEN_BUFFER_LIST: return "ACTION_OPEN_BUFFER_LIST";

// In DispatchInputTextMode:
case ACTION_OPEN_BUFFER_LIST:
    OpenBufferListModal(editor);
    break;
```

---

## Checklist

1. [ ] Fix remaining bugs (InitModalSystem line 327, strdub typo, InputSystemPollRawInput)
2. [ ] Implement layout pipeline functions + `ModalAddLayout` helper (Step 1)
3. [ ] Free `modal->layouts` in `CloseModal` and `ClearModalSystem` (Step 1e)
4. [ ] Fix `InputSystemPollRawInput` (Step 2)
5. [ ] Wire modal input routing into `EditorHandleInput` (Step 3)
6. [ ] Wire `ModalSystemRender` into `EditorRender` (Step 4)
7. [ ] Implement `BufferListState` + render/input/result/cleanup callbacks (Step 5)
8. [ ] Implement `RegisterBufferListModal` + `OpenBufferListModal` (Step 6)
9. [ ] Add `ACTION_OPEN_BUFFER_LIST` + `Ctrl+B` binding + dispatch case (Step 7)
10. [ ] Test: open editor with a file, open another buffer, press `Ctrl+B`, switch
