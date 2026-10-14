# Contributing to embedded-log

`embedded-log` is a small C11 RAM log buffer for firmware. Most of it is simple, but the interrupt path is a lock-free queue whose correctness lives in a few memory-ordering choices that no single-threaded test can see, so read [design.md](docs/design.md) before you change it.

## Getting started

Install a C11 compiler and install the Python tools with [uv](https://docs.astral.sh/uv/), as CI does. The MISRA analysis also needs a current `cppcheck`; CI installs it from Homebrew because the 2.13 release in Ubuntu 24.04 reports different findings. The same commands CI runs, locally:

```sh
# Python tools
uv tool install meson
uv tool install ninja
uv tool install gcovr
uv tool install misch

# Configure with tests and sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# ThreadSanitizer, which cannot share a build with AddressSanitizer
meson setup build-tsan --buildtype=debug -Dbuild_tests=true -Db_sanitize=thread
meson compile -C build-tsan
meson test -C build-tsan --verbose

# Release build (the configuration a downstream consumer uses)
meson setup build-rel --buildtype=release -Dbuild_tests=false
meson compile -C build-rel

# Coverage (CI gate is 100% line and branch for src/ and include/)
meson setup build-cov --buildtype=debug -Dbuild_tests=true -Db_coverage=true
meson compile -C build-cov && meson test -C build-cov
gcovr --root . --filter 'src/' --filter 'include/' --fail-under-line 100 --fail-under-branch 100 --print-summary

# MISRA C:2023, once per atomic backend (reads build/compile_commands.json)
env -u MISRA_RULE_TEXTS misch run --baseline
env -u MISRA_RULE_TEXTS misch run --baseline --profile c11
env -u MISRA_RULE_TEXTS misch run --baseline --profile no_atomics
misch deviations

# Formatting (pinned clang-format through uvx)
scripts/format.sh --fix
```

ThreadSanitizer and AddressSanitizer do not start on every AArch64 kernel; [integration.md](docs/integration.md#build-and-test) lists the limits. On such a host, run a plain native build of the full suite instead.

## Source style

- Formatting is mandatory. Run `scripts/format.sh --fix` before submitting, or `scripts/format.sh` to check without writing, as CI does. The script pins the clang-format release and runs it through `uvx`, so an editor formatter gives the same result only when it runs the same release.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions and do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `meson.build` and `tests/meson.build` when you add or remove files. No CMake, no Make, and no other build systems.

## C language rules

- C11 only.
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`, and never plain `int` for a fixed-width quantity.
- No heap allocation (`malloc`, `free`, VLAs), no system calls, and no blocking on any path.
- Validate every pointer argument at the public API boundary, and return without effect rather than fault.
- Every public identifier uses the `emlog_` or `EMLOG_` prefix, and internal macros use `EMLOG__`. New API has no `log_` forwarder.
- Reject an unusable configuration at compile time with `_Static_assert` or `#error`, rather than degrading at run time.

## Concurrency rules

- `emlog_event_isr` is the only function that may run concurrently with the others. It writes only the pending ring, `isr_head` and `isr_dropped`; `emlog_drain` writes only `isr_tail` and the main ring. Keep each index written by one side.
- Every index access goes through the `EMLOG__LOAD_*` and `EMLOG__STORE_*` macros in `src/emlog.c`, so that the backend stays selectable. Publish with a release store and observe with an acquire load; do not weaken either.
- The atomic backends must reject an index type that is not always lock-free at compile time.
- Validate any change to the interrupt path under ThreadSanitizer. A release-mode run of the concurrent test does not detect a missing ordering, even on AArch64.

## MISRA C:2023

The library is analysed with `misch` (cppcheck-backed MISRA C:2023 analysis), configured by `misra.toml`, over `include/` and `src/`. Each backend compiles different code, so the analysis runs three times: the `__atomic` backend in the base configuration, and the `c11` and `no_atomics` profiles. Each has its own baseline in `analysis/baseline/`, and CI fails on any finding that is not in the matching baseline. Run it from a configured `build/` directory, with `MISRA_RULE_TEXTS` unset, as CI does. If your change introduces a finding:

1. Prefer restructuring the code so the rule is satisfied. Deviate only when compliance would make the code genuinely worse, and never restructure purely to hide a finding from the checker; an honest deviation beats evasion.
2. Suppress at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or add a justified project-wide entry to `analysis/deviations/misra-deviations.txt` for a house-style rule.
3. Verify with all three `misch run` commands and `misch deviations`, which requires every suppression to carry a rationale, and justify the deviation in the pull request description.

Do not regenerate a baseline to admit a new finding. A baseline changes only after a deliberate review, with `misch baseline` and the same `--profile`.

## Tests and coverage

- Add a test for every bug fix and every new feature.
- Tests live in `tests/test_*.c`, one executable per concern, written with the `TEST_CASE`, `TEST_ASSERT` and `run_test` macros from `tests/test_harness.h`. Do not add Unity or another external test framework. Each new file is listed in `tests/meson.build`.
- `tests/test_legacy.c` uses only the deprecated `log.h` names; every other test uses `emlog.h`.
- `tests/test_isr.c` is built once per backend, each time against a library built with the same backend. Its concurrent case asserts that every accepted entry arrives once, in order and intact, never a timing.
- CI enforces 100 percent line and branch coverage of `src/` and `include/`. Reach new branches deterministically, in a single thread.
- The `embedded-log docs examples` test compiles every C code block in `README.md`, `CONTRIBUTING.md`, and `docs/` against the public headers, through `tests/docs/check_snippets.py`. A block compiles at file scope by default. On the line directly above the fence, put `<!-- snippet: body -->` for a block of statements, or `<!-- snippet: skip -->` for pseudo-code or a copy of a header definition.

## Documentation

| File | Purpose |
| --- | --- |
| `README.md` | Landing page: quick start, description, validated toolchains, and links. Keep it short. |
| `docs/integration.md` | Requirements, installation, configuration consistency, migration from `log.h`, building, and cross builds. |
| `docs/api.md` | Reference for the context, recording, the interrupt path, inspection, the configuration macros, and recipes. |
| `docs/design.md` | The ring, the interrupt path and its ordering, backends, context rules, failure behaviour, MISRA position, and verification. |

Update `docs/api.md` and the header Doxygen together when an interface changes, and check every value an example states against a build. Update `docs/design.md` when the ring, the interrupt path, a backend, or the verification changes. Keep it standalone: describe how the library works, not how it was built or which other projects use it. Write prose in British English, one line per paragraph.

## API stability

- The public API in `emlog.h` and the configuration macros in `emlog_conf.h` are stable within a minor release line.
- The layout of `struct emlog_entry` is part of the contract, because debuggers and post-mortem tools read it.
- Changing a function signature or the meaning of a configuration macro is a breaking change and needs a major release. So is removing the deprecated `log.h` names.
- A change to the size of `struct emlog_ctx` changes every user's RAM budget, so its `CHANGELOG.md` entry states it.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `docs: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` a change that neither fixes a bug nor adds a feature

Keep the subject under about 70 characters. Use the body to explain why the change is needed.

## Pull requests

- Open an issue first for non-trivial changes, and always for a change to the interrupt path, so the design can be agreed before implementation.
- Keep pull requests focused: one feature or one fix.
- All CI checks must pass.
- Add a `CHANGELOG.md` entry under `## [Unreleased]` for a user-visible change.
- Flag any new MISRA deviation, and any change to the interrupt path or a backend, in the description.

## When in doubt

Open an issue. The library is small, but a modest change to its ordering can break it on hardware that no CI runner uses.
