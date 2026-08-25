libjustcapture_sources = files(
  'common/errors.c',
  'common/types.c',
  'common/async-helpers.c',
  'portal/portal-client.c',
  'portal/portal-request.c',
  'portal/screenshot-portal.c',
  'portal/screencast-portal.c',
  'output/output-path.c',
  'output/filename.c',
  'environment/capabilities.c',
)

libjustcapture_headers = files(
  '../include/justcapture/justcapture.h',
  '../include/justcapture/errors.h',
  '../include/justcapture/types.h',
  '../include/justcapture/portal-client.h',
  '../include/justcapture/portal-request.h',
  '../include/justcapture/screenshot-portal.h',
  '../include/justcapture/screencast-portal.h',
  '../include/justcapture/capabilities.h',
  '../include/justcapture/output-path.h',
  '../include/justcapture/filename.h',
  '../include/justcapture/async-helpers.h',
)

libjustcapture = library('justcapture',
  libjustcapture_sources,
  version: meson.project_version(),
  soversion: '0',
  include_directories: include_directories('..', '../include'),
  dependencies: [glib_dep, gio_dep, gobject_dep],
  c_args: '-DJUSTCAPTURE_COMPILATION',
  install: true,
  )

install_headers(libjustcapture_headers, subdir: 'justcapture/')
install_headers(version_h, subdir: 'justcapture/')

libjustcapture_dep = declare_dependency(
  link_with: libjustcapture,
  include_directories: include_directories('..', '../include'),
  dependencies: [glib_dep, gio_dep, gobject_dep],
)