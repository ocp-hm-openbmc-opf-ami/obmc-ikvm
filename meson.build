project(
    'obmc-ikvm',
    'cpp',
    version: '1.0',
    license: 'GPLv2',
    default_options: ['cpp_std=c++23', 'werror=true'],
    meson_version: '>=1.1.1',
)


# Check and define macro for dual node support
if get_option('dual_node').enabled()
    add_project_arguments('-DMULTI_HOST_DEFAULT_MODE', language: 'cpp')
    message('Dual Node support is enabled')
else
    message('Dual Node support is disabled')
endif

install_data(
    'create_usbhid.sh',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir'),
)

if get_option('tests').enabled()
    add_project_arguments(
    '-Wno-error',
    '-Wno-pedantic',
    '-Wno-parentheses',
    language: 'cpp'
    )
    
    cmake = import('cmake')
    message('Building libvncserver as CMake subproject')

    libvnc_opts = cmake.subproject_options()
    libvnc_opts.add_cmake_defines({
        'BUILD_TESTS'                  : 'OFF',
        'BUILD_SHARED_LIBS'            : 'ON',
        'WITH_LZO'                     : 'OFF',
        'WITH_JPEG'                    : 'ON',
        'CMAKE_POLICY_VERSION_MINIMUM' : '3.5',
        'CMAKE_C_FLAGS'                : '-Wno-error -Wno-pedantic',
      })

    libvnc_proj = cmake.subproject(
        'libvncserver',
        options : libvnc_opts
    )

    libvnc_opts.set_override_option('cmake_dev_warnings', 'OFF')
    message('CMake targets:\n - ' + '\n - '.join(libvnc_proj.target_list()))
    libvncserver_dep = libvnc_proj.dependency('vncserver')
else
    libvncserver_dep = dependency('libvncserver')
endif

phosphor_dbus_interfaces_dep = dependency('phosphor-dbus-interfaces')

# Includes neccessary subdirectory build
subdir('ami')

executable(
    'obmc-ikvm',
    [
        'ikvm_args.cpp',
        'ikvm_input.cpp',
        'ikvm_manager.cpp',
        'ikvm_server.cpp',
        'ikvm_video.cpp',
        'obmc-ikvm.cpp',
        ami_sources,
    ],
    dependencies: [
        libvncserver_dep,
        dependency('phosphor-logging'),
        phosphor_dbus_interfaces_dep,
        dependency('sdbusplus'),
        dependency('threads'),
        dependency('boost'),
    ],
    install: true,
)

fs = import('fs')
fs.copyfile(
    'start-ipkvm.service',
    install: true,
    install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
)

# Install dual node service only if dual node is enabled
if get_option('dual_node').enabled()
    fs.copyfile(
        'start-ipkvm1.service',
        install: true,
        install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
    )
endif

if get_option('tests').enabled()
    subdir('tests')
endif
