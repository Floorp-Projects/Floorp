import sys
import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

VSPATH = '%(abs_work_dir)s/vs2015u3'
config = {
   'tooltool_manifest_file': "win.manifest",
   'exes': {
        'gittool.py': [sys.executable, os.path.join(external_tools_path, 'gittool.py')],
        'python2.7': 'c:\\mozilla-build\\python27\\python2.7.exe',
        'tooltool.py': [sys.executable, "c:\\mozilla-build\\tooltool.py"],
   },
   'dump_syms_binary': 'dump_syms.exe',
   'arch': 'x64',
   'use_yasm': True,
   'operating_system': 'msvc',
   'partial_env': {
       'PATH': '%s;%s;%s' % (
           '{_VSPATH}/VC/bin/amd64;{_VSPATH}/VC/bin;{_VSPATH}/SDK/bin/x64;{_VSPATH}/VC/redist/x64/Microsoft.VC140.CRT;{_VSPATH}/SDK/Redist/ucrt/DLLs/x64;{_VSPATH}/VC/redist/x86/Microsoft.VC140.CRT;{_VSPATH}/SDK/Redist/ucrt/DLLs/x86;{_VSPATH}/DIA SDK/bin'.format(_VSPATH=VSPATH),
           os.environ['PATH'],
           'C:\\mozilla-build\\Git\\bin',
       ),
       'WIN32_REDIST_DIR': '{_VSPATH}/VC/redist/x64/Microsoft.VC140.CRT'.format(_VSPATH=VSPATH),
       'WIN_UCRT_REDIST_DIR': '{_VSPATH}/SDK/Redist/ucrt/DLLs/x64'.format(_VSPATH=VSPATH),
       'INCLUDE': '{_VSPATH}/VC/include;{_VSPATH}/VC/atlmfc/include;{_VSPATH}/SDK/Include/10.0.14393.0/ucrt;{_VSPATH}/SDK/Include/10.0.14393.0/shared;{_VSPATH}/SDK/Include/10.0.14393.0/um;{_VSPATH}/SDK/Include/10.0.14393.0/winrt;{_VSPATH}/DIA SDK/include'.format(_VSPATH=VSPATH),
       'LIB': '{_VSPATH}/VC/lib/amd64;{_VSPATH}/VC/atlmfc/lib/amd64;{_VSPATH}/SDK/lib/10.0.14393.0/ucrt/x64;{_VSPATH}/SDK/lib/10.0.14393.0/um/x64;{_VSPATH}/DIA SDK/lib/amd64'.format(_VSPATH=VSPATH),
       'WINDOWSSDKDIR': '{_VSPATH}/SDK'.format(_VSPATH=VSPATH),
   },
}
