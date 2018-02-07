import sys
import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

VSPATH = '%(abs_work_dir)s/vs2017_15.4.2'
config = {
   'tooltool_manifest_file': "win.manifest",
   'exes': {
       'gittool.py': [sys.executable, os.path.join(external_tools_path, 'gittool.py')],
       'python2.7': 'c:\\mozilla-build\\python27\\python2.7.exe',
   },
   'dump_syms_binary': 'dump_syms.exe',
   'arch': 'x86',
   'use_yasm': True,
   'operating_system': 'msvc',
   'partial_env': {
       'PATH': '%s;%s;%s' % (
           (
               '{_VSPATH}/VC/bin/Hostx64/x86;'
               '{_VSPATH}/VC/bin/Hostx64/x64;'
               '{_VSPATH}/SDK/bin/10.0.15063.0/x64;'
               '{_VSPATH}/VC/redist/x64/Microsoft.VC141.CRT;'
               '{_VSPATH}/SDK/Redist/ucrt/DLLs/x64;'
               # 32-bit redist here for our dump_syms.exe
               '{_VSPATH}/VC/redist/x86/Microsoft.VC141.CRT;'
               '{_VSPATH}/SDK/Redist/ucrt/DLLs/x86;'
               '{_VSPATH}/DIA SDK/bin'
           ).format(_VSPATH=VSPATH),
           os.environ['PATH'],
           'C:\\mozilla-build\\Git\\bin',
       ),
       'WIN32_REDIST_DIR': '{_VSPATH}/VC/redist/x86/Microsoft.VC141.CRT'.format(_VSPATH=VSPATH),
       'WIN_UCRT_REDIST_DIR': '{_VSPATH}/SDK/Redist/ucrt/DLLs/x86'.format(_VSPATH=VSPATH),
       'INCLUDE': (
           '{_VSPATH}/VC/include;'
           '{_VSPATH}/VC/atlmfc/include;'
           '{_VSPATH}/SDK/Include/10.0.15063.0/ucrt;'
           '{_VSPATH}/SDK/Include/10.0.15063.0/shared;'
           '{_VSPATH}/SDK/Include/10.0.15063.0/um;'
           '{_VSPATH}/SDK/Include/10.0.15063.0/winrt;'
           '{_VSPATH}/DIA SDK/include'
       ).format(_VSPATH=VSPATH),
       'LIB': (
           '{_VSPATH}/VC/lib/x86;'
           '{_VSPATH}/VC/atlmfc/lib/x86;'
           '{_VSPATH}/SDK/lib/10.0.15063.0/ucrt/x86;'
           '{_VSPATH}/SDK/lib/10.0.15063.0/um/x86;'
           '{_VSPATH}/DIA SDK/lib'
        ).format(_VSPATH=VSPATH),
       'WINDOWSSDKDIR': '{_VSPATH}/SDK'.format(_VSPATH=VSPATH),
   },
}
