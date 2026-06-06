unity = subproject('unity')
unity_dep = unity.get_variable('unity_dep')

test_log = executable(
  'test_log',
  ['test_log.c'],
  dependencies: [unity_dep, embedded_log_dep],
  include_directories: [public_headers, include_directories('.')]
)

test('embedded_log_tests', test_log)

