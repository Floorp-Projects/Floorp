import sys
import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

VSPATH = '%(abs_work_dir)s\\src\\vs2017_15.9.6'
config = {
    'tooltool_manifest_file': 'win64-aarch64.manifest',
    'exes': {
        'gittool.py': [sys.executable, os.path.join(external_tools_path, 'gittool.py')],
        'python2.7': 'c:\\mozilla-build\\python\\python.exe',
    },
    'dump_syms_binary': 'dump_syms.exe',
    'arch': 'aarch64',
    'use_yasm': False,
    'partial_env': {
        'PATH': ('%(abs_work_dir)s\\openh264;'
                 '{MOZ_FETCHES_DIR}\\clang\\bin\\;'
                 '{_VSPATH}\\VC\\bin\\Hostx64\\arm64;'
                 '{_VSPATH}\\VC\\bin\\Hostx64\\x64;'
                 # 32-bit redist here for our dump_syms.exe
                 '{_VSPATH}/VC/redist/x86/Microsoft.VC141.CRT;'
                 '{_VSPATH}/SDK/Redist/ucrt/DLLs/x86;'
                 '{_VSPATH}/DIA SDK/bin;%(PATH)s;'
        ).format(_VSPATH=VSPATH, MOZ_FETCHES_DIR=os.environ['MOZ_FETCHES_DIR']),
        'INCLUDES': (
            '-I{_VSPATH}\\VC\\include '
            '-I{_VSPATH}\\VC\\atlmfc\\include '
            '-I{_VSPATH}\\SDK\\Include\\10.0.17134.0\\ucrt '
            '-I{_VSPATH}\\SDK\\Include\\10.0.17134.0\\shared '
            '-I{_VSPATH}\\SDK\\Include\\10.0.17134.0\\um '
            '-I{_VSPATH}\\SDK\\Include\\10.0.17134.0\\winrt '
        ).format(_VSPATH=VSPATH),
        'LIB': (
            '{_VSPATH}/VC/lib/arm64;'
            '{_VSPATH}/VC/atlmfc/lib/arm64;'
            '{_VSPATH}/SDK/lib/10.0.17134.0/ucrt/arm64;'
            '{_VSPATH}/SDK/lib/10.0.17134.0/um/arm64;'
        ).format(_VSPATH=VSPATH),
    },
}
