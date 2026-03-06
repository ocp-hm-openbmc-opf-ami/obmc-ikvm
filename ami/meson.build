# ============================================================
# <<<<<<<<<<<< AMI Specific Meson build Extension >>>>>>>>>>>>
# ============================================================

ami_sources = [
    'ami/src/ikvm_input_ami.cpp',
    'ami/src/ikvm_monitor.cpp',
    'ami/src/ikvm_server_ami.cpp',
    'ami/src/ikvm_utils.cpp',
    'ami/src/ikvm_video_ami.cpp',
]

image_files = ['misc/POWER_OFF.jpg', 'misc/NO_SIGNAL.jpg']
install_data(image_files, install_mode: 'r--r--r--', install_dir: '/etc/')

install_data(
    'misc/connect_dummy_ipkvm_client.sh',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir'),
)

fs = import('fs')
fs.copyfile(
    'misc/start-ipkvm.socket',
    install: true,
    install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
)

# Install dual node socket only if dual node is enabled
if get_option('dual_node').enabled()
    fs.copyfile(
        'misc/start-ipkvm1.socket',
        install: true,
        install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
    )
endif

# Install the Drop-in file only for single node (not dual node)
if not get_option('dual_node').enabled()
    fs.copyfile(
        'misc/10_ExecStop_Session_Clear.conf',
        install: true,
        install_dir: dependency('systemd').get_variable('systemdsystemunitdir') / 'start-ipkvm.service.d',
    )
endif

# Install Dummy-Client service
fs.copyfile(
    'misc/start-dummy-ipkvm-client.service',
    install: true,
    install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
)
