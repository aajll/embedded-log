project(
  'embedded-log',
  'c',
  version: '0.1.0',
  default_options: ['warning_level=3', 'c_std=c11'],
  license: 'MIT',
)

# === Version header ===========================================================

ver = meson.project_version().split('.')

major = ver[0].to_int()
minor = ver[1].to_int()
patch = ver[2].to_int()

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set('MAJOR', major)
conf.set('MINOR', minor)
conf.set('PATCH', patch)

configure_file(
  input: 'config' / 'log_version.h.in',
  output: 'log_version.h',
  configuration: conf,
  install: true,
  install_dir: get_option('includedir') / 'embedded-log',
)

# === Include directories ======================================================

public_headers = include_directories('include')

install_headers('include/log.h', subdir: 'embedded-log')

# === Application core library =================================================

library_sources = ['src/log.c']

embedded_log_lib = static_library(
  'log',
  library_sources,
  include_directories: public_headers,
  install: true,
  install_dir: get_option('libdir'),
)

embedded_log_dep = declare_dependency(
  include_directories: public_headers,
  link_with: embedded_log_lib,
)

# === pkg-config ===============================================================

pkgconfig = import('pkgconfig')
pkgconfig.generate(
  embedded_log_lib,
  name: 'embedded-log',
  description: 'Lightweight, MISRA C-compliant RAM log buffer for embedded systems',
  subdirs: 'embedded-log',
)

# === Host unit tests ==========================================================

if get_option('build_tests')
  subdir('tests')
endif
