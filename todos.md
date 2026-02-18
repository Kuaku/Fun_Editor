# Editor TODO

---

## Bug Fixes

**B.1. Fix MovePointerRight off-by-one**
Pointer can move one past end of buffer.
Dependencies: -

**B.2. Fix GotoCommand size_t underflow**
`goto 0` wraps to SIZE_MAX due to unsigned arithmetic.
Dependencies: -

**B.3. Fix FindCommand off-by-one and underflow**
Last match position never checked, potential size_t underflow when `line_length < search_length`.
Dependencies: -

**B.4. Implement missing bound actions**
`ACTION_SELECT_ALL` and `ACTION_DELETE_FORWARD` are bound but never dispatched in `DispatchInputTextMode`.
Dependencies: -

**B.5. Fix PushModalFromCache missing break**
Duplicate key pushes the same modal multiple times onto the stack.
Dependencies: -

---

## Performance

**P.1. Bulk delete in piece table**
`ExecuteDelete` is O(n²) — calls malloc+memcpy per character. Needs a single split operation for range deletes.
Dependencies: -

**P.2. Wire line cache into IndexToPosition and GenerateLine**
Both have TODO comments and iterate all pieces on every call. Every rendered frame pays this cost.
Dependencies: -

**P.3. Cache GetTextSize**
Called in tight loops, walks all pieces every time. Should be a maintained counter updated on insert/delete.
Dependencies: -

**P.4. Fix UndoStack ring buffer**
Current eviction uses memmove which is O(n). Should be a proper circular buffer with head/tail indices. Also `stack->current` is not adjusted after eviction.
Dependencies: -

---

## Features

**F.1. External hotkey mapping**
It is possible to define hotkeys and their bound actions outside of the editor in a config file.
Dependencies: -

**F.2. Hot reloading hotkey mapping**
It is possible to reload the externally defined hotkeys without restarting the editor.
Dependencies: F.1

**F.3. Action auto repeat on hold**
When an action key is held it repeats with a configurable attack delay and repeat interval. Characters already repeat at the OS level but movement and delete keys do not.
Dependencies: -

**F.4. Saving**
`ACTION_SAVE` (Ctrl+S) writes the active buffer to disk. `ACTION_SAVE_AS` (Ctrl+Shift+S) prompts for a path if none is set. Uses atomic write (temp file + rename) to avoid corruption on crash.
Dependencies: -

**F.5. Syntax highlighting**
Tokens of program files are rendered in distinct colors based on their type. Requires integration of an external tokenizer library such as Tree-sitter.
Dependencies: Tree-sitter or equivalent tokenizer library

**F.6. LSP infrastructure**
Process management and JSON-RPC transport layer that error highlighting, autocompletion, hover docs and go-to-definition can build on. Separate from any specific LSP feature.
Dependencies: -

**F.7. LSP error highlighting**
Errors and warnings reported by a connected LSP are underlined or highlighted in the editor.
Dependencies: F.6

**F.8. Autocompletion**
While typing, a popup shows completion candidates that can be confirmed with a key. Driven by LSP or a simple word-index fallback.
Dependencies: F.6

**F.9. Simple settings**
A settings modal for at minimum color scheme selection and hotkey display.
Dependencies: -

**F.10. Plugin system**
A defined API that allows external code to hook into editor events and extend behaviour without modifying the core.
Dependencies: -

**F.11. Find and replace**
Extend the existing find command with a replace operation, including replace-all.
Dependencies: -

**F.12. Split view**
Show two buffers side by side or stacked top/bottom.
Dependencies: -

**F.13. Auto indent**
When inserting a newline, match the indentation level of the previous line.
Dependencies: -

**F.14. Bracket and quote matching**
Highlight the matching bracket/paren/brace when the cursor is adjacent to one. Auto-close on insert.
Dependencies: -

**F.15. Status bar**
A persistent bottom bar showing file path, cursor position (line:col), dirty indicator, file encoding and current mode.
Dependencies: F.4 (dirty flag)

**F.16. Recent files list**
Track recently opened files, accessible from the command palette or a dedicated modal.
Dependencies: -

**F.17. Word wrap**
Optionally wrap long lines at the viewport edge instead of scrolling horizontally.
Dependencies: P.2

**F.18. Configurable tab width and indent style**
Tab width and spaces-vs-tabs behaviour are currently hardcoded to 2 spaces.
Dependencies: F.9

**F.19. UTF-8 support**
The editor currently treats text as raw bytes. Multibyte characters are not handled correctly for cursor movement, rendering or line length calculations.
Dependencies: -

**F.20. Line ending preservation**
CRLF line endings are stripped on load and always saved as LF. The original line ending style should be detected and preserved on save.
Dependencies: F.4

**F.21. Large file handling**
Loading a very large file into a single `org_buffer` is slow and memory heavy. A lazy or chunked read strategy is needed.
Dependencies: P.1, P.2

**F.22. Scrollbar**
A visual scroll indicator for vertical position and, where applicable, horizontal scroll.
Dependencies: -

**F.23. Bookmarks and jump list**
Mark positions in files and jump back and forward through the edit position history.
Dependencies: -

**F.24. LSP hover documentation**
Show a type signature or documentation popup for the symbol under the cursor, via keybind.
Dependencies: F.6

**F.25. LSP go to definition and references**
Jump to the definition of a symbol or list all its references.
Dependencies: F.6

**F.26. Git gutter indicators**
Show a colored bar in the line number margin indicating lines that are added (green), modified (yellow) or deleted (red) compared to the last commit.
Dependencies: -

**F.27. Current branch display**
Show the active branch name in the status bar alongside cursor position and file info.
Dependencies: F.15

**F.28. File status in file explorer**
Color or annotate filenames in the file explorer based on their git status — untracked, modified, staged, ignored.
Dependencies: -

**F.29. Stage and unstage file**
Stage or unstage the entire current file via a keybind.
Dependencies: -

**F.30. Commit modal**
A modal to stage/unstage individual changed files from a list, write a commit message and confirm the commit. Shows all modified, staged and untracked files so you have a full picture before committing.
Dependencies: F.29

**F.31. Commit history modal**
A modal listing recent commits on the current branch with message, author and date. Selecting a commit shows its full diff in a nested view.
Dependencies: -

**F.32. Branch switcher**
A modal to list local branches and switch between them.
Dependencies: -

**F.33. Create and delete branch**
Create a new branch or delete an existing one from within the editor.
Dependencies: F.32

**F.34. Push and pull**
Push or pull the current branch via a keybind, with stdout/stderr output shown in a result modal.
Dependencies: -

---

## Robustness

**R.1. Protect against opening the same file in two buffers**
`OpenFileFromPath` does not call `FindBufferByPath`. Opening the same file twice creates two diverging buffers.
Dependencies: -

**R.2. Dirty buffer warning on quit**
Prompt before closing if there are unsaved changes in any open buffer.
Dependencies: F.4

**R.3. File changed on disk detection**
Warn the user or offer to reload when an open file is modified externally. The polling infrastructure already tracks mtimes so the detection is close to free.
Dependencies: F.4

---

## Non-functional

**Nf.1. Restructure project**
Split the single `main.c` into the planned module structure:
`common.h`, `platform`, `statistics`, `filesystem`, `text_buffer`, `input`, `command`, `modal`, `editor`, `render`, and a `modals/` subfolder.
Dependencies: -

**Nf.2. Build system**
A proper `CMakeLists.txt` or `Makefile` with debug/release targets and cross-platform handling.
Dependencies: Nf.1

**Nf.3. Basic test harness**
Unit tests for the piece table operations, line cache, command tokenizer and undo stack. These are pure logic with no Raylib dependency and can run headless.
Dependencies: Nf.1

**Nf.4. Separate Position struct uses**
`Position` is used as both screen coordinates and `{start_offset, length}` in the line cache. Introduce a dedicated `LineInfo` struct to remove the ambiguity.
Dependencies: Nf.1