# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
