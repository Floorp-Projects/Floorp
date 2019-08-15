import sys
import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

VSPATH = '%(abs_work_dir)s\\src\\vs2017_15.8.4'
config = {
    'tooltool_manifest_file': 'win.manifest',
    'exes': {
        'gittool.py': [sys.executable, os.path.join(external_tools_path, 'gittool.py')],
        'python2.7': 'c:\\mozilla-build\\python\\python.exe',
    },
    'dump_syms_binary': 'dump_syms.exe',
    'arch': 'x86',
    'use_yasm': True,
    'partial_env': {
        'PATH': ('{MOZ_FETCHES_DIR}\\clang\\bin\\;'
                 '{_VSPATH}\\VC\\bin\\Hostx64\\x64;%(PATH)s'
                 # 32-bit redist here for our dump_syms.exe
                 '{_VSPATH}/VC/redist/x86/Microsoft.VC141.CRT;'
                 '{_VSPATH}/SDK/Redist/ucrt/DLLs/x86;'
                 '{_VSPATH}/DIA SDK/bin'
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
            '{_VSPATH}/VC/lib/x86;'
            '{_VSPATH}/VC/atlmfc/lib/x86;'
            '{_VSPATH}/SDK/lib/10.0.17134.0/ucrt/x86;'
            '{_VSPATH}/SDK/lib/10.0.17134.0/um/x86;'
        ).format(_VSPATH=VSPATH),
    },
}
