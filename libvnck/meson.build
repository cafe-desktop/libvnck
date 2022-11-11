gnome = import('gnome')

libvnck_includedir = join_paths(PACKAGE_NAME, meson.project_name())

version_data = configuration_data()
version_array = meson.project_version().split('.')
version_data.set('VNCK_MAJOR_VERSION', version_array[0])
version_data.set('VNCK_MINOR_VERSION', version_array[1])
version_data.set('VNCK_MICRO_VERSION', version_array[2])

headers = [
  'libvnck.h',
  'pager.h',
  'application.h',
  'class-group.h',
  'screen.h',
  'selector.h',
  'tasklist.h',
  'util.h',
  'window.h',
  'window-action-menu.h',
  'workspace.h',
]

headers += configure_file(
  input: 'version.h.in',
  output: 'version.h',
  configuration: version_data)

sources = [
  'application.c',
  'class-group.c',
  'pager.c',
  'screen.c',
  'selector.c',
  'tasklist.c',
  'util.c',
  'window-action-menu.c',
  'window.c',
  'vnck-image-menu-item-private.h',
  'vnck-image-menu-item.c',
  'workspace.c',
  'xutils.c',
]

a11y_sources = [
  'pager-accessible.c',
  'pager-accessible.h',
  'pager-accessible-factory.c',
  'pager-accessible-factory.h',
  'workspace-accessible.c',
  'workspace-accessible.h',
  'workspace-accessible-factory.c',
  'workspace-accessible-factory.h',
]

enum_types = gnome.mkenums_simple('vnck-enum-types',
  sources : headers,
  install_header: true,
  install_dir: join_paths(includedir, libvnck_includedir)
)

resources = gnome.compile_resources(
  '@0@-resources'.format(meson.project_name()),
  'vnck.gresource.xml',
  source_dir: '.',
  c_name: meson.project_name()
)

libvnck_cflags = [
  '-DG_LOG_DOMAIN="Vnck"',
  '-DVNCK_I_KNOW_THIS_IS_UNSTABLE',
  '-DVNCK_LOCALEDIR="@0@"'.format(localedir),
  '-DVNCK_COMPILATION',
  '-DSN_API_NOT_YET_FROZEN=1'
]

mapfile = MODULE_NAME + '.map'
libvnck_ldflags = [
  '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile),
]

if get_option('deprecation_flags')
  foreach domain: ['G', 'ATK', 'GDK', 'GDK_PIXBUF', 'CTK', 'VNCK']
    libvnck_cflags += '-D@0@_DISABLE_DEPRECATED'.format(domain)
  endforeach
endif

libvnck_dep = declare_dependency(
  include_directories: default_includes,
  dependencies: LIBVNCK_DEPS,
  compile_args: libvnck_cflags,
  sources: headers + [enum_types[1]],
  link_args: libvnck_ldflags,
)

libvnck_lib = shared_library(LIBNAME,
  dependencies: libvnck_dep,
  sources: sources + a11y_sources + enum_types + resources,
  version: '@0@.@1@.@2@'.format(LIBVNCK_SOVERSION, LIBVNCK_CURRENT, LIBVNCK_REVISION),
  soversion: LIBVNCK_SOVERSION,
  link_depends: mapfile,
  install: true,
)

introspection = get_option('introspection')
if not introspection.disabled()
  find_program('g-ir-scanner', required: introspection.enabled())
  gnome.generate_gir(libvnck_lib,
    sources: headers + sources + enum_types,
    namespace: 'Vnck',
    nsversion: MODULE_VERSION,
    export_packages: PACKAGE_NAME,
    includes: ['GObject-2.0', 'GdkPixbuf-2.0', 'Ctk-3.0'],
    extra_args: '--c-include=@0@/@0@.h'.format(meson.project_name()),
    install: true
  )
endif

install_headers(headers, subdir: libvnck_includedir)

progs = [
  'vnckprop',
]

if not get_option('deprecation_flags')
  progs += 'vnck-urgency-monitor'
endif

test_progs = [
  'test-vnck',
  'test-tasklist',
  'test-selector',
  'test-pager',
  'test-urgent',
  'test-shutdown',
]

foreach prog: progs + test_progs
  executable(prog, [prog + '.c'],
    include_directories: default_includes,
    dependencies: libvnck_dep,
    link_with: libvnck_lib,
    install: progs.contains(prog) and get_option('install_tools'),
    install_dir: bindir)
endforeach
