project(
    'obmc-ikvm',
    'cpp',
    version: '1.0',
    license: 'GPLv2',
    default_options: [
        'cpp_std=c++20',
        'werror=true',
    ],
    meson_version: '>=0.58.0',
)

install_data(
    'create_usbhid.sh',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir')
)

executable(
    'obmc-ikvm',
    [
        'ikvm_args.cpp',
        'ikvm_input.cpp',
        'ikvm_interface.cpp',
        'ikvm_manager.cpp',
        'ikvm_monitor.cpp',
        'ikvm_server.cpp',
        'ikvm_utils.cpp',
        'ikvm_video.cpp',
        'obmc-ikvm.cpp',
    ],
    dependencies: [
        dependency('libvncserver'),
        dependency('phosphor-logging'),
        dependency('phosphor-dbus-interfaces'),
        dependency('sdbusplus'),
        dependency('threads'),
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