# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- `emlog.h`, `emlog_conf.h` and a generated `emlog_version.h` provide the API under the `emlog_` and `EMLOG_` prefixes (`struct emlog_ctx`, `emlog_event()`, `EMLOG_INFO`, `EMLOG_ONCE`, `EMLOG_MSG_LEN`), so it can be included alongside `<syslog.h>`.
- `emlog_event_isr()` records an entry from interrupt context with a lock-free single-producer enqueue, deferring formatting to `emlog_drain()` in the background; `emlog_get_isr_dropped()` counts entries lost to a full pending ring. `EMLOG_ISR_ENTRIES` sizes the pending ring, and `EMLOG_USE_GNU_ATOMICS`, `EMLOG_USE_C11_ATOMICS` or `EMLOG_USE_NO_ATOMICS` override the atomic backend.
- CI runs the interrupt-path tests under ThreadSanitizer and builds them once per atomic backend.
- CI cross-builds the library with TI C2000 CGT 22.6.2 and 25.11.1 under the COFF and EABI object formats, with warnings as errors.
- The build registers `embedded-log` with `meson.override_dependency`, so other subprojects in a consumer's build find it by name.
- `scripts/format.sh` checks or applies formatting with a pinned clang-format release run through `uvx`, and CI runs the check.
- CI runs a MISRA C:2023 analysis with `misch` for each atomic backend, against baselines in `analysis/baseline/` that record the existing findings.

### Changed

- CI installs Meson, Ninja and gcovr with `uv` instead of `pip` and `apt`.
- The Meson files and the test harness carry SPDX licence headers.
- Tests are split into one executable per concern (initialisation, event recording, ring reads, deprecated names), and CI gates line and branch coverage at 100 %.

### Deprecated

- `log.h`, `log_conf.h` and `log_version.h` and the unprefixed names they define. They forward to the `emlog` API, so existing code compiles unchanged, and will be removed in the next major release.

### Fixed

- The `EMLOG_ONCE` (and `LOG_ONCE`) guard is `uint_least8_t`, so the macro compiles where `CHAR_BIT == 16` and `uint8_t` does not exist.

- `log_version.h` now defines `LOG_VERSION` as a single string literal and `LOG_VERSION_MAJOR`, `_MINOR` and `_PATCH` as integers, so the macros compile when used.

## [1.0.0] - 2026-06-12

### Added

- Added `log_conf.h` configuration header, extracting `LOG_MSG_LEN` and `LOG_ENTRIES` macros for easier user overrides.
- Added the initial changelog, contributor guide, security policy, standard CI workflow, installed version header, pkg-config metadata, and SPDX headers.
- Lightweight RAM log buffer for embedded systems with configurable entry count, message length, log levels, `LOG_ONCE`, defensive NULL handling, no dynamic memory, and Meson subproject integration.

### Changed

- Renamed the log level enumerators to `LOG_INFO`, `LOG_WARN`, and `LOG_FAULT` to avoid collisions with other headers.
- Replaced the Unity test framework with a small in-tree harness (`tests/test_harness.h`), removing the only external dependency.
- Reworked `LOG_ONCE` to strict ISO C11 (no GNU `##__VA_ARGS__` extension) and documented its concurrency caveat.
- `log_event` now stores an empty, NUL-terminated message if formatting fails, instead of indeterminate buffer contents.
- Documented that `log_get_buffer` returns entries in physical (storage) order, and the accepted MISRA C:2023 deviations (Rules 17.1 and 21.6).
- Renamed the test directory to `tests/`, standardised README badges and ignore rules, and aligned Meson defaults with the primitive family baseline.

### Fixed

- `src/log.c` now includes `"log.h"` rather than a relative path, so the documented copy-in installation compiles.
- Corrected the documented `LOG_ENTRIES` / `LOG_MSG_LEN` defaults (both `64U`) in the README and `log_conf.h`.
- The test suite now compiles with clang (a GCC-only nested function previously broke the macOS CI build).
