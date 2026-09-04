gtest_dep = dependency('gtest', main: false, required: false)
gmock_dep = dependency('gmock', required: false)

if not gtest_dep.found() or not gmock_dep.found()
    cmake = import('cmake')
    opt_var = cmake.subproject_options()
    opt_var.add_cmake_defines({'CMAKE_CXX_FLAGS': '-Wno-pedantic'})
    opt_var.set_override_option('warning_level', '3')
    opt_var.set_override_option('werror', 'true')

    gtest_proj = cmake.subproject('gtest', options: opt_var, required: true)
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [dependency('threads'), gtest_proj.dependency('gtest')]
        )
        gmock_dep = gtest_proj.dependency('gmock')
    else
        assert(
            not get_option('tests').enabled(),
            'Googletest is required if tests are enabled',
        )
    endif
endif

phosphor_logging_dep = dependency('phosphor-logging')

common_deps = [
    gtest_dep,
    gmock_dep,
    libvncserver_dep,
    phosphor_dbus_interfaces_dep,
    phosphor_logging_dep,
    dependency('sdbusplus'),
    dependency('threads'),
]

common_link_args = [
    '-lcrypt',
    '-lstdc++fs',
]

common_cpp_args = [
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DTEST',
]

args_sources = [
    '../ikvm_args.cpp',
    '../ami/src/ikvm_utils.cpp',
]

utils_sources = [
    '../ami/src/ikvm_utils.cpp',
]

input_sources = [
    '../ikvm_args.cpp',
    '../ikvm_input.cpp',
    '../ikvm_server.cpp',
    '../ikvm_video.cpp',
    '../ami/src/ikvm_utils.cpp',
    '../ami/src/ikvm_input_ami.cpp',
    '../ami/src/ikvm_video_ami.cpp',
    '../ami/src/ikvm_server_ami.cpp',
]

server_sources = [
    '../ikvm_args.cpp',
    '../ikvm_input.cpp',
    '../ikvm_video.cpp',
    '../ikvm_server.cpp',
    '../ami/src/ikvm_utils.cpp',
    '../ami/src/ikvm_input_ami.cpp',
    '../ami/src/ikvm_video_ami.cpp',
    '../ami/src/ikvm_server_ami.cpp',
]

video_sources = [
    '../ikvm_input.cpp',
    '../ikvm_video.cpp',
    '../ami/src/ikvm_utils.cpp',
    '../ami/src/ikvm_input_ami.cpp',
    '../ami/src/ikvm_video_ami.cpp',
]

test_args_exe = executable(
    'test_args',
    [
        'test_main.cpp',
        'test_args.cpp',
    ] + args_sources,
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: common_link_args,
    cpp_args: common_cpp_args,
)
test('test_args', test_args_exe)

test_utils_exe = executable(
    'test_utils',
    [
        'test_main.cpp',
        'test_utils.cpp',
    ] + utils_sources,
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: common_link_args,
    cpp_args: common_cpp_args,
)
test('test_utils', test_utils_exe)

test_input_exe = executable(
    'test_input',
    [
        'test_main.cpp',
        'test_input.cpp',
    ] + input_sources,
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: common_link_args,
    cpp_args: common_cpp_args,
)
test('test_input', test_input_exe)

test_server_exe = executable(
    'test_server',
    [
        'test_main.cpp',
        'test_server.cpp',
    ] + server_sources,
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: common_link_args,
    cpp_args: common_cpp_args,
)
test('test_server', test_server_exe)

test_video_exe = executable(
    'test_video',
    [
        'test_main.cpp',
        'test_video.cpp',
    ] + video_sources,
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: common_link_args,
    cpp_args: common_cpp_args,
)
test('test_video', test_video_exe)
