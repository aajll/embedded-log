# embedded-log

[![CI](https://github.com/aajll/embedded-log/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/embedded-log/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**A RAM log buffer for C11 firmware without stdout, a UART or a file system: entries go into a caller-owned ring that a debugger can read live or after a crash.**

## Quick start

Copy `include/emlog.h`, `include/emlog_conf.h` and `src/emlog.c` into your project. For a Meson subproject or an installed `pkg-config` package, see the [integration guide](docs/integration.md).

```c
#include <stdio.h>

static struct emlog_ctx app_log;
static uint32_t ticks;

/* Any monotonic source; the unit is yours. */
static uint32_t
get_ticks(void)
{
        return ticks;
}

int
main(void)
{
        emlog_init(&app_log, get_ticks);

        ticks = 10u;
        emlog_event(&app_log, EMLOG_INFO, "boot, mode=%d", 2);
        for (int i = 0; i < 3; ++i) {
                EMLOG_ONCE(&app_log, EMLOG_WARN, "sensor not ready");
        }

        /* Oldest first: "[10] 0 boot, mode=2", "[10] 1 sensor not ready". */
        for (uint16_t i = 0u; i < emlog_get_count(&app_log); ++i) {
                const struct emlog_entry *e = emlog_get_entry(&app_log, i);
                printf("[%lu] %u %s\n", (unsigned long)e->timestamp,
                       (unsigned)e->level, e->msg);
        }
        return 0;
}
```

## Description

Each entry holds a timestamp from a function you supply, a level, and a `printf`-style message formatted into a fixed-size buffer. When the ring is full, a new entry replaces the oldest.

- All storage lives in a `struct emlog_ctx` that you own, sized at compile time; no heap and no OS
- `emlog_event()` formats with `vsnprintf`, truncating to `EMLOG_MSG_LEN - 1` characters, and `EMLOG_ONCE` records a message at most once per call site per reset
- `emlog_event_isr()` records from an interrupt without locking or formatting, and `emlog_drain()` formats those entries later in the background
- The interrupt path runs on GCC/Clang `__atomic`, C11 `<stdatomic.h>`, or a single-core `volatile` backend selected at compile time
- Every name carries the `emlog_` or `EMLOG_` prefix, so the header coexists with `<syslog.h>`; `log.h` remains as a deprecated forwarder
- Tests under ASan, UBSan and ThreadSanitizer with 100% line and branch coverage, and MISRA C:2023 analysis with [`misch`](https://pypi.org/project/misch/)

## Validated toolchains

| Toolchain               | Target                        | Validation                                                     |
| ----------------------- | ----------------------------- | -------------------------------------------------------------- |
| GCC                     | x86_64 Linux                  | All tests under ASan and UBSan, and under ThreadSanitizer       |
| DSP vendor C11 compiler | 16-bit MAU (`CHAR_BIT == 16`) | Library compile with the `volatile` backend, two object formats |

Target linking and execution belong to the consuming firmware. See [cross builds](docs/integration.md#cross-builds).

## Documentation

- [Integration guide](docs/integration.md) — copy-in, Meson subproject, installed package, configuration, migrating from `log.h`, building, and cross builds
- [API reference](docs/api.md) — the context, recording, the interrupt path, inspection, configuration macros, and recipes
- [Design](docs/design.md) — the ring, the interrupt path and its memory ordering, backends, context rules, failure behaviour, and verification
- [Changelog](CHANGELOG.md) — release history

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open a pull request. Report security issues as described in [SECURITY.md](.github/SECURITY.md).

## License

MIT. See [LICENSE](LICENSE).
