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
