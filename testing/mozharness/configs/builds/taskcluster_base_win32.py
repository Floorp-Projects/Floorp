import os

config = {
    'platform': 'win32',
    'env': {
        'PDBSTR_PATH': 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x86/srcsrv/pdbstr.exe',
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_src_dir)s\\win32-minidump_stackwalk.exe',
        'MINIDUMP_SAVE_PATH': os.path.join(os.getcwd(), 'public', 'build'),
    },
    'mozconfig_platform': 'win32',
}
