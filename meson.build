project(
    'obmc-ikvm',
    'cpp',
    version: '1.0',
    license: 'GPLv2',
    default_options: [
        'cpp_std=c++23',
        'werror=true',
    ],
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
    install_dir: get_option('bindir')
)

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
        dependency('libvncserver'),
        dependency('phosphor-logging'),
        dependency('phosphor-dbus-interfaces'),
        dependency('sdbusplus'),
        dependency('threads'),
        dependency('boost'),
    ],
    install: true
)

fs = import('fs')
fs.copyfile(
    'start-ipkvm.service',
    install: true,
    install_dir:
        dependency('systemd').get_variable(
            'systemdsystemunitdir')
)

# Install dual node service only if dual node is enabled
if get_option('dual_node').enabled()
    fs.copyfile(
        'start-ipkvm1.service',
        install: true,
        install_dir:
            dependency('systemd').get_variable(
                'systemdsystemunitdir')
    )
endif
