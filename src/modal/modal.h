#ifndef MODAL_H
#define MODAL_H

#include "../common.h"
#include "../input/input.h"

typedef struct Modal Modal;

typedef void (*LayoutFunc)(Modal* modal);
typedef void (*ModalRenderFunc)(Modal* modal, Rect content_bounds);
typedef void (*ModalInputFunc)(Modal* modal, RawInput input);
typedef void (*ModalCleanupFunc)(void* state);
typedef void (*ModalResultCallback)(Modal* modal, bool confirmed, void* result, void* user_data);
typedef void (*ModalUpdateFunc)(Modal* modal);

typedef struct {
    Color background;
    Color border;
    Color text;
    Color text_muted;
    Color selection;
    Color input_background;
    Color focused_border;
    int border_width;
    Position content_padding;
    int widget_spacing;
    int title_height;
    Position title_padding;
    bool draw_title;
} ModalStyle;

struct Modal {
    char* title;

    Rect bounds;
    Position wanted_size;
    Position min_size;
    Position max_size;
    Position margin;

    ModalStyle style;

    LayoutFunc* layouts;
    size_t layout_count;
    size_t layout_capacity;

    bool is_cached;

    ModalRenderFunc custom_render;
    ModalUpdateFunc custom_update;
    ModalInputFunc custom_input;
    ModalCleanupFunc cleanup;
    ModalRepeatableFunc is_repeatable;

    ModalResultCallback on_result;
    void* on_result_user_data;
    void* result_data;

    void* state;
};

typedef struct {
    char** modal_keys;
    Modal** modal_cache;
    size_t cache_count;
    size_t cache_capacity;

    Modal** stack;
    size_t stack_count;
    size_t stack_capacity;

    ModalStyle default_style;

    Font font;
    int font_size;

    int screen_width;
    int screen_height;
} ModalSystem;

void ApplyWantedSize(Modal* modal);
void ApplyMaxSize(Modal* modal);
void ApplyMinSize(Modal* modal);
void ApplyMargin(Modal* modal);
void CenterModal(Modal* modal);
void ModalAddLayout(Modal* modal, LayoutFunc layout);

ModalSystem InitModalSystem(void);
void ClearModalSystem(ModalSystem* system);

Modal* CreateModal(ModalSystem* system, const char* title, Position wanted_size, ModalRenderFunc render, ModalUpdateFunc update, ModalInputFunc input, ModalCleanupFunc cleanup, ModalRepeatableFunc is_repeatable, void* state);
void ClearModal(Modal* modal);

void PushModal(ModalSystem* system, Modal* modal);
void CloseModal(ModalSystem* system, bool confirmed);
Modal* GetTopModal(ModalSystem* system);

void PushModalFromCache(ModalSystem* system, const char* key);
void RegisterModalToQuickCatch(ModalSystem* system, const char* key, Modal* modal);

bool ModalSystemHasActive(ModalSystem* system);
void ModalSystemRender(Editor* editor);

#endif
