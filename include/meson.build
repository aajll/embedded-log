# Generated here so it lands in the build tree's include/ directory, which
# public_headers already exposes to dependants.
configure_file(
  input: files('..' / 'config' / 'emlog_version.h.in'),
  output: 'emlog_version.h',
  configuration: conf,
  install: true,
  install_dir: get_option('includedir') / 'embedded-log',
)
