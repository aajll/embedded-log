project(
  'embedded-log',
  'c',
  version: '1.0.0',
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

subdir('include')

# === Include directories ======================================================

public_headers = include_directories('include')

# emlog_* are the API; log_* are deprecated forwarders kept for existing
# callers until the next major release.
install_headers(
  [
    'include/emlog.h',
    'include/emlog_conf.h',
    'include/log.h',
    'include/log_conf.h',
    'include/log_version.h',
  ],
  subdir: 'embedded-log',
)

# === Application core library =================================================

library_sources = files('src/emlog.c')

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
  description: 'Lightweight, RAM log buffer for embedded systems',
  subdirs: 'embedded-log',
)

# === Host unit tests ==========================================================

if get_option('build_tests')
  subdir('tests')
endif
