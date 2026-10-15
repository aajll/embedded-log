# Integration guide

This guide shows how to add `embedded-log` to a project, configure it, build it, and run its tests. The API is described in [api.md](api.md), and the ring, the interrupt path and their guarantees in [design.md](design.md).

## Requirements

- A C11 compiler and a hosted or freestanding C library that provides `vsnprintf` and `snprintf` (`<stdio.h>`), `<stdarg.h>` and `memset`.
- One of the three atomic backends for the interrupt path: GCC or Clang `__atomic` builtins, C11 `<stdatomic.h>`, or a single core on which the interrupt pre-empts the context that drains the log. The header selects one automatically; [design.md](design.md#backends) sets out what each guarantees.
- A timestamp function, `uint32_t (*)(void)`, that is monotonic in whatever unit you choose.
- For the tests: Meson, Ninja, Python 3, and POSIX threads.

## Copy-in

Copy three files into your source tree:

```
include/emlog.h
include/emlog_conf.h
src/emlog.c
```

Keep both headers in the same directory, because `emlog.h` includes `emlog_conf.h`. Then include the public header:

```c
#include "emlog.h"
```

Code that still uses the deprecated names also needs `include/log.h` and `include/log_conf.h`; see [migrating from `log.h`](#migrating-from-logh). `emlog.h` defines the version macros described in [api.md](api.md#version), and `log_version.h` is a deprecated header that includes it.

## Meson subproject

Put the repository in `subprojects/embedded-log`, as a checkout or through a wrap file, then declare the dependency:

```meson
embedded_log_dep = dependency('embedded-log', fallback: ['embedded-log', 'embedded_log_dep'])
```

The build calls `meson.override_dependency('embedded-log', ...)`, so other subprojects in the same build find `embedded-log` by name. `embedded_log_dep` links the static library and adds the include directory. Tests are off unless the `build_tests` option is set.

| Option | Default | Effect |
| --- | --- | --- |
| `build_tests` | `false` | Builds the unit tests, the interrupt-path tests for each backend, and the documentation example test. |

## Installed package

```sh
meson setup build --buildtype=release -Dbuild_tests=false --prefix=/usr/local
meson compile -C build
meson install -C build
cc app.c $(pkg-config --cflags --libs embedded-log) -o app
```

The install contains the static archive `liblog.a`, the headers in an `embedded-log/` subdirectory, and `embedded-log.pc`. The `pkg-config` flags add that directory to the include path, so the same include works as in a copy-in build. Without `pkg-config`, include the namespaced path instead:

<!-- snippet: skip -->
```c
#include <embedded-log/emlog.h>
```

## Configuration

Every option in `emlog_conf.h` sits behind an `#ifndef` guard, so a `-D` definition replaces the shipped default. The options are described in [api.md](api.md#configuration).

`EMLOG_ENTRIES`, `EMLOG_MSG_LEN` and `EMLOG_ISR_ENTRIES` set the size of the arrays in `struct emlog_ctx`, and the backend selection macros set the type of its pending-ring indices. Define them identically for `src/emlog.c` and for every file that includes `emlog.h`. Nothing detects a mismatch: a caller built with `-DEMLOG_ENTRIES=16` against a library built with the default of 64 compiles and links, and the library then writes past the end of the caller's context.

A definition placed in a caller's source before `#include "emlog.h"` changes only that caller, so it creates exactly that mismatch. `emlog.h` includes `emlog_conf.h` with quotes, so the compiler finds the copy beside `emlog.h` before any directory on the include path, and a project cannot replace it with its own `emlog_conf.h` either. Use `-D` definitions that reach every file.

In a Meson build, set them in the top-level project's `c_args`, which also apply to subprojects:

```sh
meson setup build "-Dc_args=-DEMLOG_ENTRIES=128 -DEMLOG_MSG_LEN=48"
```

A per-target `c_args` or an `add_project_arguments` call in the parent project does not reach the subproject, and an installed library keeps the values it was built with.

## Migrating from `log.h`

`log.h`, `log_conf.h` and `log_version.h` are deprecated forwarding headers, so code written against the unprefixed names compiles unchanged. They map each original name onto its prefixed form with a macro, and a `LOG_ENTRIES` or `LOG_MSG_LEN` definition still sets the matching `EMLOG_` value. They will be removed in the next major release.

| Deprecated | Replacement |
| --- | --- |
| `#include "log.h"` | `#include "emlog.h"` |
| `struct log_ctx`, `struct log_entry`, `enum log_level` | `struct emlog_ctx`, `struct emlog_entry`, `enum emlog_level` |
| `log_init`, `log_event`, `log_get_count`, `log_get_entry`, `log_get_buffer` | `emlog_init`, `emlog_event`, `emlog_get_count`, `emlog_get_entry`, `emlog_get_buffer` |
| `LOG_INFO`, `LOG_WARN`, `LOG_FAULT`, `LOG_ONCE`, `LOG_PRINTF_LIKE` | `EMLOG_INFO`, `EMLOG_WARN`, `EMLOG_FAULT`, `EMLOG_ONCE`, `EMLOG_PRINTF_LIKE` |
| `LOG_ENTRIES`, `LOG_MSG_LEN`, `LOG_VERSION*` | `EMLOG_ENTRIES`, `EMLOG_MSG_LEN`, `EMLOG_VERSION*` |

The forwarders are object-like macros, so `log.h` renames those identifiers throughout any file that includes it, and it still collides with `<syslog.h>`. The interrupt path has no deprecated names. The same consistency rule applies to `LOG_ENTRIES` and `LOG_MSG_LEN` as to their `EMLOG_` forms.

## Build and test

```sh
# Library only
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# All tests under ASan and UBSan
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# The same tests under ThreadSanitizer
meson setup build-tsan --buildtype=debug -Dbuild_tests=true -Db_sanitize=thread
meson compile -C build-tsan
meson test -C build-tsan --verbose
```

The test build compiles the library once more for each atomic backend, because the interrupt-path test and the library must select the same one. Where `<syslog.h>` exists, a further test includes it beside `emlog.h`.

ThreadSanitizer and AddressSanitizer each need a large virtual address space. They run on x86-64 and on AArch64 kernels with 4 KiB pages and a 48-bit address space. ThreadSanitizer cannot start on a 16 KiB-page AArch64 kernel, and AddressSanitizer cannot start on a kernel with a 39-bit address space; run a plain native build there to exercise the weak-memory path.

Coverage, formatting and MISRA commands are in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Cross builds

The consuming firmware owns its cross file, linker command file, and memory placement. No cross file ships with this project.

CI compiles the library with warnings as errors using two releases of a vendor C11 compiler for a 16-bit-MAU DSP target, under both of its object formats. That compiler provides neither GCC builtins nor `<stdatomic.h>`, so it selects the `volatile` backend. This covers the source dialect and type assumptions only; linking and execution remain checks for the consumer. Read the [`volatile` backend](design.md#volatile-backend) limits before you use it.

On a target where `int` is 16 bits, the `%d` and `%u` conversions take 16-bit arguments. Use the `<inttypes.h>` macros for fixed-width values in `emlog_event()`, and always for `emlog_event_isr()`, whose arguments are `uint32_t`.
