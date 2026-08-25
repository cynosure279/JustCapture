test_errors = executable('test-errors',
  'test-errors.c',
  link_with: libjustcapture,
  include_directories: include_directories('..', '../include'),
  dependencies: [glib_dep, gio_dep],
)
test('test-errors', test_errors)

test_filename = executable('test-filename',
  'test-filename.c',
  link_with: libjustcapture,
  include_directories: include_directories('..', '../include'),
  dependencies: [glib_dep],
)
test('test-filename', test_filename, is_parallel: false)

test_output_path = executable('test-output-path',
  'test-output-path.c',
  link_with: libjustcapture,
  include_directories: include_directories('..', '../include'),
  dependencies: [glib_dep, gio_dep],
)
test('test-output-path', test_output_path, is_parallel: false)

test_types = executable('test-types',
  'test-types.c',
  link_with: libjustcapture,
  include_directories: include_directories('..', '../include'),
  dependencies: [glib_dep, gio_dep],
)
test('test-types', test_types)
