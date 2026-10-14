# test_log.c is written against the deprecated log.h names and checks that
# existing callers keep working through the forwarding headers.
test_log = executable(
  'test_log',
  ['test_log.c', 'test_harness.c'],
  dependencies: [embedded_log_dep],
  include_directories: include_directories('.'),
)
test('embedded_log_tests', test_log)

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
