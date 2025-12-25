# AGENTS.md

## Overview
This repository provides a lightweight logging library for embedded systems.  The code is built with **Meson**, unit‑tested with **Unity**, and formatted using **clang‑format**.  The following document collects the commands agents need to build, lint, and test the project, as well as the coding conventions used throughout the codebase.

---

# Build, Lint & Test Commands

## 1. Building the project
```bash
# Create a build directory (once)
meson setup builddir
# Build the library and test executable
meson compile -C builddir
```
*The default build type is `debug`.  For a release build add `--buildtype=release` to `meson setup`.*

## 2. Running the full test suite
```bash
meson test -C builddir --print-errorlogs
```
This runs **all** test executables defined in `test/meson.build` (currently `test_log`).

## 3. Running a single test executable
```bash
# Run the test binary directly (fastest)
./builddir/test/test_log
```
or via Meson (useful for Meson’s test‑filtering features)
```bash
meson test -C builddir test_log
```
Both commands execute the Unity test runner contained in `test_log.c` which runs every `RUN_TEST` entry.

## 4. Running a single Unity test case
Unity supports the `-t` (test name) flag.  Use the binary directly:
```bash
./builddir/test/test_log -t test_log_init_and_event
```
Replace the name after `-t` with any function that begins with `test_`.

## 5. Linting / Formatting
The project adheres to the `.clang-format` file located at the repository root.
```bash
# Check formatting (dry‑run)
clang-format -style=file -output-replacements-xml $(git ls-files "*.[ch]") | grep -q '<replacement '
# Apply formatting in‑place
clang-format -i -style=file $(git ls-files "*.[ch]")
```
Optionally run `cppcheck` for static analysis:
```bash
cppcheck --enable=warning,style,performance,portability src/include test src
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

## 2. Header Organization
- Guard macros use the full filename in uppercase with `_H` suffix (e.g. `LOG_H`).
- All public symbols are declared in `include/`.  Private helpers stay in `src/`.
- Group related declarations (enums, structs, prototypes) together and precede them with a Doxygen block comment.
- The header ends with a single `#endif /* LOG_H */` line.

## 3. Naming Conventions
| Element | Convention | Example |
|---------|------------|---------|
| **Macros / constants** | `UPPER_SNAKE_CASE` | `LOG_MSG_LEN`, `LOG_ENTRIES` |
| **Enum values** | Uppercase, prefixed with enum name if ambiguous | `INFO`, `WARN`, `FAULT` |
| **Struct types** | `struct_name_t` or plain `struct name` | `struct log_entry` |
| **Functions** | `snake_case` (lowercase, underscores) | `log_init`, `log_event` |
| **Static/file‑local variables** | `lower_snake_case` (no leading underscore) | `static uint8_t _logged` is *exception* – static locals that act as flags may start with underscore.
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
- Use `static` variables for state that must survive across calls (e.g. `LOG_ONCE` flag) and keep them confined to the smallest possible scope.

## 6. Macro Guidelines
- **`LOG_ONCE`** is the only public macro.  It follows the pattern `UPPER_SNAKE_CASE` and expands to a `do { … } while (0)` block.
- Do **not** place side‑effects in macro arguments; always evaluate arguments exactly once.
- Avoid multi‑statement macros without the `do … while (0)` wrapper.

## 7. Documentation
- All public symbols must have a Doxygen comment block (`/** … */`).
- Brief description on the first line, detailed description optional.
- Use `@param` and `@return` tags for functions.
- Group related items with `@defgroup` and `@{ … @}` as shown in `log.h`.

## 8. Testing Conventions (Unity)
- Test files live in `test/`.  Each test file includes `unity.h` *before* the library header.
- Every test function is named `test_<feature>`.  The `main()` function calls `UNITY_BEGIN()`, `RUN_TEST(...)` for each test, and `UNITY_END()`.
- To add a new test:
  1. Write a `static void test_<name>(void)` function.
  2. Add a `RUN_TEST(test_<name>);` line before `return UNITY_END();`.
  3. Increment the Meson test count if a new executable is added (not required for a new function inside an existing test file).
- Use `TEST_ASSERT_*` macros for validation.  Prefer the most specific macro (`*_UINT16`, `*_STRING`, etc.).

## 9. Meson Build Guidelines
- The top‑level `meson.build` defines the static library `log` and declares a dependency `embedded_log_dep`.
- Unit tests are enabled with the option `build_tests` (default `true`).  The `test/meson.build` file adds the Unity subproject and creates an executable `test_log` linked against `embedded_log_dep`.
- When adding new source files, update the `sources:` array in the `static_library` declaration.
- When adding new include directories, modify the `include_directories()` call.

---

# Cursor / Copilot Rules
No `.cursor/` or `.github/copilot‑instructions.md` files exist in this repository, so there are no additional agent‑specific rules to enforce.

---

*Generated for use by autonomous coding agents.  Keep this file up‑to‑date as the project evolves.*
