project('libjustcapture', 'c',
  version: '0.1.0',
  meson_version: '>= 1.3.0',
  license: 'LGPL-2.1+',
  default_options: ['warning_level=2', 'c_std=c17'])

glib_dep = dependency('glib-2.0', version: '>= 2.80')
gio_dep = dependency('gio-2.0', version: '>= 2.80')
gobject_dep = dependency('gobject-2.0', version: '>= 2.80')

conf = configuration_data()
ver_split = meson.project_version().split('.')
conf.set('JUSTCAPTURE_MAJOR', ver_split[0])
conf.set('JUSTCAPTURE_MINOR', ver_split[1])
conf.set('JUSTCAPTURE_MICRO', ver_split[2])
conf.set('JUSTCAPTURE_VERSION', meson.project_version())

version_h = configure_file(
  input: 'include/justcapture/version.h.in',
  output: 'version.h',
  configuration: conf,
)

subdir('src')
subdir('tests')
