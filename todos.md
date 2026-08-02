# Editor — TODO

Status legend: ✅ done · 🟡 partial · ⬜ not started

---

## Bug Fixes

| ID  | Status | Item | Notes | Deps |
|-----|--------|------|-------|------|
| B.1 | ✅ | `MovePointerRight` off-by-one | Now guards `pointer_position < text_buffer_size` before advancing. | — |
| B.2 | ✅ | `GotoCommand` `size_t` underflow | `goto 0` returns early via the `numb_value == 0` check. | — |
| B.3 | ✅ | `FindCommand` off-by-one & underflow | Loop bound is now inclusive (`j <= line_length - search_length`) and `line_length < search_length` is guarded. | — |
| B.4 | ✅ | Missing bound actions dispatched | `ACTION_SELECT_ALL` and `ACTION_DELETE_FORWARD` are now handled in `DispatchInputTextMode`. | — |
| B.5 | ✅ | `PushModalFromCache` missing break | Returns after the first matching key; no duplicate pushes. | — |

---

## Performance

| ID  | Status | Item | Notes | Deps |
|-----|--------|------|-------|------|
| P.1 | ⬜ | Bulk delete in piece table | `ExecuteDelete` still does a per-range rebuild; range deletes should be a single split. | — |
| P.2 | ⬜ | Wire line cache into `IndexToPosition` / `GenerateLine` | Both still iterate all pieces on every call; cost is paid every rendered frame. | — |
| P.3 | ⬜ | Cache `GetTextSize` | Walks all pieces every call; should be a maintained counter updated on insert/delete. | — |
| P.4 | ⬜ | Fix `UndoStack` ring buffer | Eviction uses O(n) `memmove`; needs a real circular buffer with head/tail, and `stack->current` must be adjusted on eviction. | — |

---

## Features

| ID   | Status | Item | Notes | Deps |
|------|--------|------|-------|------|
| F.1  | ⬜ | External hotkey mapping | Define hotkeys + bound actions in a config file. | — |
| F.2  | ⬜ | Hot reload hotkey mapping | Reload external hotkeys without restarting. | F.1 |
| F.3  | ✅ | Action auto-repeat on hold | Held action keys repeat with a configurable attack delay and interval (`held_action` in `InputSystemPoll`). | — |
| F.4  | 🟡 | Saving | `ACTION_SAVE` (Ctrl+S) writes the active buffer with atomic temp-file + rename. `ACTION_SAVE_AS` (Ctrl+Shift+S, prompt for path) not yet added. | — |
| F.5  | ⬜ | Syntax highlighting | Color tokens by type; needs an external tokenizer (Tree-sitter or equivalent). | Tokenizer lib |
| F.6  | ⬜ | LSP infrastructure | Process management + JSON-RPC transport, independent of any specific LSP feature. | — |
| F.7  | ⬜ | LSP error highlighting | Underline/highlight diagnostics from a connected LSP. | F.6 |
| F.8  | ⬜ | Autocompletion | Completion popup confirmed with a key; LSP-driven or word-index fallback. | F.6 |
| F.9  | ⬜ | Simple settings | Settings modal for color scheme and hotkey display. | — |
| F.10 | ⬜ | Plugin system | Public API for hooking editor events without touching core. | — |
| F.11 | ⬜ | Find and replace | Extend `find` with replace and replace-all. | — |
| F.12 | ⬜ | Split view | Two buffers side-by-side or stacked. | — |
| F.13 | ⬜ | Auto indent | Match previous line's indentation on newline. | — |
| F.14 | ⬜ | Bracket & quote matching | Highlight the matching bracket near the cursor; auto-close on insert. | — |
| F.15 | 🟡 | Status bar | A bottom bar already shows file path + cursor position (line:col) and mode. Still missing: dirty indicator and file encoding. | F.4 (dirty flag) |
| F.16 | ⬜ | Recent files list | Track recent files, reachable from palette or a modal. | — |
| F.17 | ⬜ | Word wrap | Wrap long lines at the viewport edge. | P.2 |
| F.18 | ⬜ | Configurable tab width / indent style | Currently hardcoded to 2 spaces. | F.9 |
| F.19 | 🟡 | UTF-8 support | UTF-8 cursor movement, continuation handling, and codepoint-aware positions are implemented across `text_buffer`, `command`, and the modals via `utf8.*`. Verify rendering and line-length edge cases before calling this fully done. | — |
| F.20 | ⬜ | Line ending preservation | CRLF is stripped on load and always saved as LF; detect and preserve original style. | F.4 |
| F.21 | ⬜ | Large file handling | Lazy/chunked read instead of one big `org_buffer`. | P.1, P.2 |
| F.22 | ⬜ | Scrollbar | Visual vertical (and where relevant horizontal) scroll indicator. | — |
| F.23 | ⬜ | Bookmarks & jump list | Mark positions and jump back/forward through edit history. | — |
| F.24 | ⬜ | LSP hover documentation | Type/doc popup for the symbol under the cursor. | F.6 |
| F.25 | ⬜ | LSP go-to-definition & references | Jump to definition or list references. | F.6 |
| F.26 | ⬜ | Git gutter indicators | Added/modified/deleted bars in the line-number margin. | — |
| F.27 | ⬜ | Current branch display | Show active branch in the status bar. | F.15 |
| F.28 | ⬜ | File status in file explorer | Annotate filenames by git status. | — |
| F.29 | ⬜ | Stage / unstage file | Stage or unstage the current file via keybind. | — |
| F.30 | ⬜ | Commit modal | Stage/unstage from a list, write a message, confirm the commit. | F.29 |
| F.31 | ⬜ | Commit history modal | List recent commits; select to view full diff. | — |
| F.32 | ⬜ | Branch switcher | List and switch local branches. | — |
| F.33 | ⬜ | Create / delete branch | Manage branches from the editor. | F.32 |
| F.34 | ⬜ | Push / pull | Push/pull current branch with output in a result modal. | — |

---

## Robustness

| ID  | Status | Item | Notes | Deps |
|-----|--------|------|-------|------|
| R.1 | 🟡 | Prevent opening the same file twice | `OpenOrSwitchToFile` (used by the file explorer) now de-dupes via `FindBufferByPath`. The `open` command still calls `OpenFileFromPath` directly — route it through `OpenOrSwitchToFile` to finish. | — |
| R.2 | ⬜ | Dirty-buffer warning on quit | Prompt before closing if any buffer has unsaved changes. | F.4 |
| R.3 | ⬜ | File-changed-on-disk detection | Warn/offer reload on external edits; polling already tracks mtimes, so detection is nearly free. | F.4 |

---

## Non-functional

| ID   | Status | Item | Notes | Deps |
|------|--------|------|-------|------|
| Nf.1 | ✅ | Restructure project | Single `main.c` split into modules: `common.h`, `platform`, `statistics`, `filesystem`, `text_buffer`, `input`, `command`, `modal`, `editor`, `render`, plus a `modals/` subfolder. | — |
| Nf.2 | 🟡 | Build system | `build.bat` (Windows) and `Makefile` (Linux) updated for the new structure. Debug/release targets not yet added. | Nf.1 |
| Nf.3 | ⬜ | Basic test harness | Headless unit tests for piece table, line cache, command tokenizer, and undo stack (no Raylib dependency). | Nf.1 |
| Nf.4 | ⬜ | Separate `Position` uses | `Position` doubles as screen coordinates and `{start_offset, length}` in the line cache; introduce a `LineInfo` struct to remove the ambiguity. | Nf.1 |

---

## Summary

- **Bug fixes:** all 5 resolved (B.1–B.5).
- **Performance:** none started (P.1–P.4).
- **Features:** F.3 done; F.4, F.15, F.19 partial; rest open.
- **Robustness:** R.1 partial; R.2, R.3 open.
- **Non-functional:** Nf.1 done; Nf.2 partial; Nf.3, Nf.4 open.