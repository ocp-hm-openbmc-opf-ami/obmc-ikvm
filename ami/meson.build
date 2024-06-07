# ============================================================
# <<<<<<<<<<<< AMI Specific Meson build Extension >>>>>>>>>>>>
# ============================================================

ami_sources = [
    'ami/src/ikvm_input_ami.cpp',
    'ami/src/ikvm_interface.cpp',
    'ami/src/ikvm_monitor.cpp',
    'ami/src/ikvm_server_ami.cpp',
    'ami/src/ikvm_utils.cpp',
    'ami/src/ikvm_video_ami.cpp',
]

image_files = [
    'misc/POWER_OFF.jpg',
    'misc/NO_SIGNAL.jpg',
]
install_data(
    image_files,
    install_mode: 'r--r--r--',
    install_dir: '/etc/'
)

fs = import('fs')
fs.copyfile(
    'misc/start-ipkvm.socket',
    install: true,
    install_dir:
        dependency('systemd').get_variable(
            'systemdsystemunitdir')
)