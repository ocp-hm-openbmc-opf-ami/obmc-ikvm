kvm_dbus_utils_test_exe = executable(
    'kvm_dbus_utils_tests',
    [
        '../../tests/test_main.cpp',
        'kvm_dbus_utils_test.cpp',
        '../src/kvm_dbus-utils.cpp',
    ],
    include_directories: include_directories('../include'),
    dependencies: [
        gtest_dep,
        gmock_dep,
        phosphor_logging_dep,
        dependency('boost'),
        dependency('sdbusplus'),
        dependency('threads'),
    ],
    cpp_args: common_cpp_args + [
        '-DTEST',
        '-DKVM_DBUS_TEST_JSON_PATH="/tmp/kvm-dbus-monitor.json"',
        '-DKVM_DBUS_TEST_MOUNTS="/tmp/kvm-dbus-mounts.txt"',
    ],
)

test('kvm_dbus_utils_test', kvm_dbus_utils_test_exe, protocol: 'gtest')

kvm_dbus_interface_test_exe = executable(
    'kvm_dbus_interface_tests',
    [
        '../../tests/test_main.cpp',
        'kvm_dbus_interface_test.cpp',
        '../src/kvm_dbus-interface.cpp',
        '../src/kvm_dbus-utils.cpp',
    ],
    include_directories: include_directories('../include'),
    dependencies: [
        gtest_dep,
        gmock_dep,
        phosphor_logging_dep,
        dependency('boost'),
        dependency('sdbusplus'),
        dependency('threads'),
    ],
    cpp_args: common_cpp_args + [
        '-DTEST',
        '-DKVM_DBUS_TEST_JSON_PATH="/tmp/kvm-dbus-interface-test.json"',
        '-DKVM_DBUS_TEST_MOUNTS="/tmp/kvm-dbus-interface-mounts.txt"',
        '-DIKVM_UT_DROPIN_DIR_PREFIX="/tmp/ikvm-kvm-dbus-interface"',
    ],
)

kvm_dbus_monitor_test_exe = executable(
    'kvm_dbus_monitor_tests',
    [
        '../../tests/test_main.cpp',
        'kvm_dbus_monitor_test.cpp',
        '../src/kvm_dbus-monitor.cpp',
        '../src/kvm_dbus-utils.cpp',
    ],
    include_directories: include_directories('../include'),
    dependencies: [
        gtest_dep,
        gmock_dep,
        phosphor_logging_dep,
        dependency('boost'),
        dependency('sdbusplus'),
        dependency('threads'),
    ],
    cpp_args: common_cpp_args + [
        '-DTEST',
        '-DKVM_DBUS_TEST_JSON_PATH="/tmp/kvm-dbus-monitor-test.json"',
        '-DKVM_DBUS_TEST_MOUNTS="/tmp/kvm-dbus-monitor-mounts.txt"',
        '-DIKVM_UT_DROPIN_DIR_PREFIX="/tmp/ikvm-kvm-dbus-monitor"',
    ],
)

kvm_dbus_run_session = find_program('dbus-run-session', required: false)
if kvm_dbus_run_session.found()
    test(
        'kvm_dbus_interface_test',
        kvm_dbus_run_session,
        args: [kvm_dbus_interface_test_exe],
        protocol: 'gtest',
    )
    test(
        'kvm_dbus_monitor_test',
        kvm_dbus_run_session,
        args: [kvm_dbus_monitor_test_exe],
        protocol: 'gtest',
    )
else
    warning('Skipping kvm_dbus_interface_test: dbus-run-session not found')
    warning('Skipping kvm_dbus_monitor_test: dbus-run-session not found')
endif