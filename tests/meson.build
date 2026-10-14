# SPDX-License-Identifier: MIT

# One executable per concern. Each links the library and the in-tree
# harness.
foreach t : ['init', 'event', 'read', 'legacy']
  exe = executable(
    'test_' + t,
    ['test_' + t + '.c', 'test_harness.c'],
    dependencies: [embedded_log_dep],
    include_directories: include_directories('.'),
  )
  test(t, exe)
endforeach

# The interrupt path, once per atomic backend. The library and the test
# must agree on the backend, so each gets its own build of the library.
# Hosts with the C11 backend run the concurrent case as well.
threads_dep = dependency('threads')
foreach backend : ['GNU', 'C11', 'NO']
  isr_args = ['-DEMLOG_USE_' + backend + '_ATOMICS=1']
  isr_lib = static_library(
    'log_isr_' + backend.to_lower(),
    library_sources,
    c_args: isr_args,
    include_directories: public_headers,
  )
  exe = executable(
    'test_isr_' + backend.to_lower(),
    ['test_isr.c', 'test_harness.c'],
    c_args: isr_args,
    link_with: isr_lib,
    dependencies: [threads_dep],
    include_directories: [include_directories('.'), public_headers],
  )
  test('isr_' + backend.to_lower(), exe, timeout: 120)
endforeach

# Header-only: the overridden configuration never reaches the library.
test_legacy_conf = executable(
  'test_legacy_conf',
  ['test_legacy_conf.c'],
  include_directories: public_headers,
)
test('legacy_conf', test_legacy_conf)

if meson.get_compiler('c').has_header('syslog.h')
  test_syslog = executable(
    'test_syslog',
    ['test_syslog.c', 'test_harness.c'],
    dependencies: [embedded_log_dep],
    include_directories: include_directories('.'),
  )
  test('syslog_coexistence', test_syslog)
endif
