test_log = executable(
  'test_log',
  ['test_log.c', 'test_harness.c'],
  dependencies: [embedded_log_dep],
  include_directories: include_directories('.'),
)

test('embedded_log_tests', test_log)
