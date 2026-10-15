# SPDX-License-Identifier: MIT

project(
  'embedded-log',
  'c',
  version: '1.0.0',
  meson_version: '>=0.57.0',
  default_options: ['warning_level=3', 'c_std=c11'],
  license: 'MIT',
)

# === Version ==================================================================
# include/emlog.h defines the version macros, so a copy-in build has them.
# Configuration fails when they differ from the project version.

fs = import('fs')
version_parts = meson.project_version().split('.')
version_macros = {
  'EMLOG_VERSION_MAJOR': version_parts[0],
  'EMLOG_VERSION_MINOR': version_parts[1],
  'EMLOG_VERSION_PATCH': version_parts[2],
  'EMLOG_VERSION': '"' + meson.project_version() + '"',
}
header_lines = []
foreach line : fs.read('include' / 'emlog.h').split('\n')
  header_lines += ' '.join(line.split())
endforeach
foreach name, value : version_macros
  if '#define @0@ @1@'.format(name, value) not in header_lines
    error('include/emlog.h must define @0@ as @1@'.format(name, value))
  endif
endforeach

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

meson.override_dependency('embedded-log', embedded_log_dep)

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
