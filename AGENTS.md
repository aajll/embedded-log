# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `embedded-log`
**Primary goal:** A C11 RAM log buffer for firmware: timestamped, levelled, `printf`-formatted entries in a caller-owned ring that a debugger can read, plus a lock-free single-producer path that records from an interrupt and defers formatting to the owning context. No allocation, no system calls, no locks.

### 1.1 Essential commands

#### Configure and build (library only)

```sh
meson setup build --wipe --buildtype=release -Dbuild_tests=false
meson compile -C build
```

#### Configure, build, and run unit tests

```sh
meson setup build --wipe --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

#### Concurrency check (ThreadSanitizer)

```sh
meson setup build-tsan --wipe --buildtype=debug -Dbuild_tests=true -Db_sanitize=thread
meson test -C build-tsan --verbose
```

#### MISRA C:2023, once per atomic backend (reads `build/compile_commands.json`)

```sh
env -u MISRA_RULE_TEXTS misch run --baseline
env -u MISRA_RULE_TEXTS misch run --baseline --profile c11
env -u MISRA_RULE_TEXTS misch run --baseline --profile no_atomics
misch deviations
```

#### Formatting (pinned clang-format through `uvx`)

```sh
scripts/format.sh          # check, as CI does
scripts/format.sh --fix    # rewrite in place
```

### 1.2 Notes

- `include/emlog.h` defines the version macros, and `meson setup` fails when they differ from `project(version:)`, so a release updates both.
- The build produces only a static library, installed as `liblog.a` with headers under `embedded-log/` and `embedded-log.pc`. The only Meson option is `build_tests`.
- `EMLOG_ENTRIES`, `EMLOG_MSG_LEN`, `EMLOG_ISR_ENTRIES` and the backend macros must be identical for the library and every caller; nothing detects a mismatch. `emlog.h` includes `emlog_conf.h` with quotes, so configuration is by `-D` only.
- `log.h`, `log_conf.h` and `log_version.h` are deprecated forwarders. Do not add new names to them.
- ThreadSanitizer needs a 4 KiB-page kernel on AArch64, and AddressSanitizer does not start on a 39-bit address space; use a plain native build there.

---

## 2) Non-negotiable design invariants

- **One owner.** `emlog_event`, `emlog_drain`, `EMLOG_ONCE` and the inspection functions run in one context and are not synchronised with each other.
- **Single producer.** `emlog_event_isr` is the only function that may run concurrently with the owner. It writes only the pending ring, `isr_head` and `isr_dropped`; `emlog_drain` writes only `isr_tail` and the main ring. Each index has one writer, so no read-modify-write is needed.
- **Ordering.** The producer publishes a slot with a release store of `isr_head`, and the consumer frees it with a release store of `isr_tail`; each side observes the other's index with an acquire load. Never weaken either.
- **Selectable backend.** Every index access goes through the `EMLOG__LOAD_*` and `EMLOG__STORE_*` macros. The atomic backends reject an index type that is not always lock-free at compile time.
- **Deferred formatting.** The interrupt path stores the format pointer and two `uint32_t` arguments, never formats, and never touches the main ring.
- **Bounded output.** Every formatted message is truncated to `EMLOG_MSG_LEN - 1` characters and terminated; an encoding error stores an empty message.
- No heap, no system calls, no blocking, no global state other than the `EMLOG_ONCE` flags.

---

## 3) CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs (see §1.1 above).
- Install Python tools (Meson, Ninja, gcovr, misch) with `uv tool install`, never `pip`; CI does the same through `astral-sh/setup-uv`. CI installs `cppcheck` from Homebrew because Ubuntu's 2.13 reports different findings.
- `misch run --baseline` must pass for the base configuration and both profiles; each has its own baseline in `analysis/baseline/`. Never regenerate a baseline to admit a new finding. Run it with `MISRA_RULE_TEXTS` unset, as CI does.
- The CI jobs are: tests under ASan/UBSan, the same tests under ThreadSanitizer, a release build, a 100 percent line and branch coverage gate, a TI C2000 library build (CGT 22.6.2 and 25.11.1, COFF and EABI, warnings as errors), `clang-format`, and `misch` for each backend.

---

## 4) Docs / commit conventions

### Documentation layout

- `README.md` is a short landing page (under 100 lines). Put detail in `docs/`, never in the README; link to the owning file instead.
- `docs/integration.md` owns requirements, install methods, configuration consistency, migration from `log.h`, building, sanitiser host limits, and cross builds.
- `docs/api.md` owns the API reference, the configuration macros and recipes. Change it together with the headers; keep signatures in fenced `c` blocks and describe them in prose, not function tables. Check every value an example states against a build.
- `docs/design.md` is the single authoritative design record: the ring, the interrupt path and its ordering, backends and their limits, context rules, failure behaviour, MISRA position, and verification.
- `CONTRIBUTING.md` documents the contributor workflow; `CHANGELOG.md` holds release history (Keep a Changelog); `.github/SECURITY.md` covers vulnerability reporting.
- Code blocks in `README.md`, `CONTRIBUTING.md`, and `docs/` are compiled by the `embedded-log docs examples` test (`tests/docs/check_snippets.py`). Each document is compiled as one source file: a `c` fence is file-level code, `<!-- snippet: body -->` wraps statements, and `<!-- snippet: skip -->` excludes pseudo-code. Do not use `skip` to silence a broken example.
- Prose uses British spelling. Do not hard-wrap Markdown: write one line per paragraph. The 80-column limit applies to C source, not Markdown.

### Commits

- Use **Conventional Commits** format when asked to commit.
- Keep commits focused; explain *why* in the message body.

---

## 5) C style expectations

### Build & configuration

- Use the Meson build system. Do not introduce CMake, Make, or other systems.
- Update `meson.build` and `tests/meson.build` when adding or removing files.
- `emlog_conf.h` is public and installed beside `emlog.h`. Every option in it stays behind an `#ifndef` guard.

### Formatting

- `.clang-format` is present and **mandatory**. Run `scripts/format.sh --fix` before committing. It runs the clang-format release pinned in the script through `uvx`. Do not use a system `clang-format`: another release can format the same code differently.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Style & correctness

- Match conventions in the existing files (indentation, braces, naming).
- Every public identifier uses the `emlog_` or `EMLOG_` prefix; internal macros use `EMLOG__`. Keep public headers minimal; do not expose internal helpers.
- Validate pointer arguments at every public API boundary and return without effect; no `errno`.
- Use fixed-width types from `<stdint.h>`; never plain `int` for a fixed-width quantity.
- Reject an unusable configuration with `_Static_assert` or `#error` rather than degrading at run time.
- Function-like macros expand to a `do { ... } while (0)` block and evaluate each argument once.
- Public symbols carry Doxygen with the full contract: preconditions, concurrency, and return semantics.

### MISRA deviations

- Prefer compliant code; never restructure only to hide a finding. Deviate at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or project-wide in `analysis/deviations/misra-deviations.txt` for a house-style rule.

### Comment placement (Doxygen)

- Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit.
- If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries.
- Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line.
- After editing, run `scripts/format.sh` and an 80-column scan.

### Testing

- Run `meson test -C build` after every change, and the ThreadSanitizer build for any change to the interrupt path or a backend.
- Tests use the harness in `tests/test_harness.h` (`TEST_CASE`, `TEST_ASSERT`, `run_test`), one executable per concern. Do not add Unity or another external framework.
- `tests/test_legacy.c` uses only the deprecated names; every other test uses `emlog.h`.
- Concurrency tests assert an invariant, never a timing. Reach new branches deterministically in a single thread.
- Add a test case for each bug fix and each new feature.
- Keep `src/` and `include/` at the coverage gate (100 percent line and branch).
