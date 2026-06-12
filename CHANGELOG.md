# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-06-12

### Added

- Added `log_conf.h` configuration header, extracting `LOG_MSG_LEN` and `LOG_ENTRIES` macros for easier user overrides.
- Added the initial changelog, contributor guide, security policy, standard CI workflow, installed version header, pkg-config metadata, and SPDX headers.
- Lightweight RAM log buffer for embedded systems with configurable entry count, message length, log levels, `LOG_ONCE`, defensive NULL handling, no dynamic memory, and Meson subproject integration.

### Changed

- Renamed the test directory to `tests/`, standardised README badges and ignore rules, and aligned Meson defaults with the primitive family baseline.
