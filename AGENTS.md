# AGENTS.md

## Overview
This repository provides a lightweight logging library for embedded systems.  The code is built with **Meson**, unit‑tested with a **small in‑tree harness** (`tests/test_harness.h`), and formatted using **clang‑format**.  There are no external test dependencies.  The following document collects the commands agents need to build, lint, and test the project, as well as the coding conventions used throughout the codebase.

---

# Build, Lint & Test Commands

## 1. Building the project
```bash
# Create a build directory (once); tests are off by default
meson setup builddir -Dbuild_tests=true
# Build the library and test executable
meson compile -C builddir
```
*The default build type is `debug`.  For a release build add `--buildtype=release` to `meson setup`.*

## 2. Running the full test suite
```bash
meson test -C builddir --print-errorlogs
```
This runs **all** test executables defined in `tests/meson.build`: one per concern (`test_init`, `test_event`, `test_read`, `test_legacy`), `test_isr_gnu`, `test_isr_c11` and `test_isr_no` (the interrupt path built against a library per atomic backend), plus `test_legacy_conf` and, where `<syslog.h>` exists, `test_syslog`.

## 3. Running a single test executable
```bash
# Run the test binary directly (fastest)
./builddir/tests/test_event
```
or via Meson (useful for Meson’s test‑filtering features)
```bash
meson test -C builddir event
```
Both commands execute the in‑tree test runner contained in `test_event.c`, which runs every `run_test` entry registered in `main()`.

## 4. Linting / Formatting
The project adheres to the `.clang-format` file located at the repository root.
```bash
# Check formatting (dry‑run)
clang-format -style=file -output-replacements-xml $(git ls-files "*.[ch]") | grep -q '<replacement '
# Apply formatting in‑place
clang-format -i -style=file $(git ls-files "*.[ch]")
```
Optionally run `cppcheck` for static analysis:
```bash
cppcheck --enable=warning,style,performance,portability include tests src
```

---

# Code Style Guidelines
These guidelines are enforced by the `.clang-format` file and by the project’s coding philosophy.  Agents should follow them when generating or modifying code.

## 1. General Formatting (clang‑format)
- **Indent width:** 8 spaces (tab width is also 8, but `UseTab: Never`).
- **Column limit:** 80 characters.
- **Brace style:** `BreakBeforeBraces: Linux` – opening brace on the same line for functions, after control statements *never* (`AfterControlStatement: Never`).
- **Always use braces** for `if`, `for`, `while`, and `switch` statements (`InsertBraces: true`).
- **No one‑line `if`/`else`/`while` bodies** – `AllowShortIfStatements: Never`.
- **Trailing comments** are aligned (`AlignTrailingComments: true`).
- **Include directives** are sorted alphabetically (`SortIncludes: true`).
- **Binary operators** break before the operator (`BreakBeforeBinaryOperators: NonAssignment`).
- **String literals** may be broken (`BreakStringLiterals: true`).

## 2. Header Organisation
- Guard macros use the full filename in uppercase with `_H` suffix (e.g. `EMLOG_H`).
- All public symbols are declared in `include/`.  Private helpers stay in `src/`.
- Group related declarations (enums, structs, prototypes) together and precede them with a Doxygen block comment.
- The header ends with a single `#endif /* EMLOG_H */` line.

## 3. Naming Conventions
| Element | Convention | Example |
|---------|------------|---------|
| **Macros / constants** | `UPPER_SNAKE_CASE` | `EMLOG_MSG_LEN`, `EMLOG_ENTRIES` |
| **Enum values** | `UPPER_SNAKE_CASE`, always prefixed with the module name | `EMLOG_INFO`, `EMLOG_WARN`, `EMLOG_FAULT` |
| **Struct types** | `struct_name_t` or plain `struct name` | `struct emlog_entry` |
| **Functions** | `snake_case` (lowercase, underscores) | `emlog_init`, `emlog_event` |
| **Static/file‑local variables** | `lower_snake_case` (no leading underscore) | `static uint_least8_t emlog_once_done` |
| **Parameters** | `lower_snake_case` | `struct log_ctx *ctx` |
| **Typedefs** | `PascalCase` ending with `_t` if appropriate | `typedef uint32_t timestamp_t;` |

## 4. Types & Qualifiers
- Use fixed‑width integer types (`uint8_t`, `uint16_t`, `uint32_t`).
- All public APIs use `const` where the data is not modified (`const char *fmt`).
- Function pointers are `uint32_t (*timestamp_fn)(void)`.
- Do **not** use `void*` unless absolutely necessary.
- Prefer `enum` for categorical values (log level).

## 5. Error Handling & Defensive Programming
- Validate public pointers: if a pointer argument may be `NULL`, the function must either handle it gracefully or assert and return early.
    ```c
    if (ctx == NULL) return; // safe‑guard
    ```
- `log_event` must ignore a `NULL` format string – the existing implementation does this.
- All public functions return `void` unless an error code is meaningful.  If an error must be propagated, use an `int` return where `0` = success, non‑zero = error.
- Use `static` variables for state that must survive across calls (e.g. `EMLOG_ONCE` flag) and keep them confined to the smallest possible scope.

## 6. Macro Guidelines
- **`EMLOG_ONCE`** is the only function-like macro in the API.  It follows the pattern `UPPER_SNAKE_CASE` and expands to a `do { … } while (0)` block.  Configuration macros live in `emlog_conf.h`; internal helpers use the `EMLOG__` prefix.
- `emlog_event_isr()` is the only function that may run concurrently with the others, from a single producer.  Its indices go through the `EMLOG__LOAD_*`/`EMLOG__STORE_*` backend macros in `src/emlog.c`; keep release on publish and acquire on observe.
- Do **not** place side‑effects in macro arguments; always evaluate arguments exactly once.
- Avoid multi‑statement macros without the `do … while (0)` wrapper.

## 7. Documentation
- All public symbols must have a Doxygen comment block (`/** … */`).
- Brief description on the first line, detailed description optional.
- Use `@param` and `@return` tags for functions.
- Group related items with `@defgroup` and `@{ … @}` as shown in `emlog.h`.

## 7.1 Member comment placement (Doxygen)

Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit. If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries. Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line. After editing, verify with a `clang-format --style=file` no-reformat diff and an 80-column scan.

## 8. Testing Conventions (in‑tree harness)
- Test files live in `tests/`, one file and executable per concern.  Each test file includes `test_harness.h` *before* the library header.
- `test_legacy.c` uses only the deprecated `log.h` names; every other test uses `emlog.h`.
- CI gates line and branch coverage at 100 % and runs the suite under ThreadSanitizer.  Every new branch needs a test that fails when the branch is broken.
- Test cases are defined with `TEST_CASE(test_<feature>)` and registered in `main()` with `run_test(test_<feature>, "test_<feature>");`.
- Use the `TEST_ASSERT(expr)` macro for validation; a failed assertion prints the file, line, and expression, then exits non‑zero.
- Tests must compile as strict ISO C11 — no GNU extensions (nested functions, `##__VA_ARGS__`, etc.).  CI builds them with both GCC and clang.

## 9. Meson Build Guidelines
- The top‑level `meson.build` defines the static library `log` and declares a dependency `embedded_log_dep`.
- Unit tests are enabled with the option `build_tests` (default `false`).  The `tests/meson.build` file builds one executable per concern from `test_<concern>.c` and `test_harness.c`, linked against `embedded_log_dep`.
- When adding new source files, update the `sources:` array in the `static_library` declaration.
- When adding new include directories, modify the `include_directories()` call.

---

# Cursor / Copilot Rules
No `.cursor/` or `.github/copilot‑instructions.md` files exist in this repository, so there are no additional agent‑specific rules to enforce.

---

*Generated for use by autonomous coding agents.  Keep this file up‑to‑date as the project evolves.*
