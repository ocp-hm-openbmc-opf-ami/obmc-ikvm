project(
    'kvm-support',
    'cpp',
    version: '1.0',
    license: 'GPLv2',
    default_options: [
        'cpp_std=c++20',
        'werror=true',
    ],
    meson_version: '>=1.1.1',
)

cpp = meson.get_compiler('cpp')

executable(
    'kvm-support',
    [   
        'src/kvm_support-interface.cpp', 
        'src/kvm-support.cpp',
    ],
    include_directories: ['include'],
    dependencies: [
        dependency('phosphor-logging'),
        dependency('phosphor-dbus-interfaces'),
        dependency('sdbusplus'),
        dependency('threads'),
        dependency('boost'),
    ],
    link_args: ['-lcrypt', '-lstdc++fs'],
    cpp_args: ['-DBOOST_ALL_NO_LIB', '-DBOOST_SYSTEM_NO_DEPRECATED', '-DBOOST_ERROR_CODE_HEADER_ONLY'],
    install: true,
)

fs = import('fs')
fs.copyfile(
    'kvm-support.service',
    install: true,
    install_dir:
        dependency('systemd').get_variable(
            'systemdsystemunitdir')
)
