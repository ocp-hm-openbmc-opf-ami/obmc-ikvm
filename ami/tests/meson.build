monitor_test_exe = executable(
    'test_monitor',
    [
        '../../tests/test_main.cpp',
        'test_monitor.cpp',
        '../src/ikvm_monitor.cpp',
        '../src/ikvm_utils.cpp',
    ],
    include_directories: include_directories('../..'),
    dependencies: common_deps,
    link_args: common_link_args,
    cpp_args: common_cpp_args,
)

dbus_run_session = find_program('dbus-run-session', required: false)
if dbus_run_session.found()
    test(
        'test_monitor',
        dbus_run_session,
        args: [monitor_test_exe],
        protocol: 'gtest',
    )
else
    warning('Skipping test_monitor: dbus-run-session not found')
endif