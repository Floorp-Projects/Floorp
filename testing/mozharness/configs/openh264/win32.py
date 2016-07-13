import sys
import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

VSPATH = 'C:/tools/vs2013'
config = {
   'tooltool_manifest_file': "win.manifest",
   'exes': {
       'gittool.py': [sys.executable, os.path.join(external_tools_path, 'gittool.py')],
       'python2.7': 'c:\\mozilla-build\\python27\\python2.7.exe',
       'tooltool.py': [sys.executable, "c:\\mozilla-build\\tooltool.py"],
   },
   'dump_syms_binary': 'dump_syms_vc1800.exe',
   'arch': 'x86',
   'use_yasm': True,
   'operating_system': 'msvc',
   'partial_env': {
       'PATH': '%s;%s;%s;%s;%s' % (
           'c:/Program Files (x86)/Windows Kits/8.1/bin/x86;{_VSPATH}/Common7/IDE;{_VSPATH}/VC/BIN/amd64_x86;{_VSPATH}/VC/BIN/amd64;{_VSPATH}/Common7/Tools;{_VSPATH}/VC/VCPackages;c:/mozilla-build/moztools'.format(_VSPATH=VSPATH),
           'c:/windows/Microsoft.NET/Framework/v3.5;c:/windows/Microsoft.NET/Framework/v4.0.30319',
           os.environ['PATH'],
           'C:\\mozilla-build\\Git\\bin',
           'C:\\mozilla-build\\svn-win32-1.6.3\\bin',
       ),
       'WIN32_REDIST_DIR': '{_VSPATH}/VC/redist/x86/Microsoft.VC120.CRT'.format(_VSPATH=VSPATH),
       'INCLUDE': 'c:\\Program Files (x86)\\Windows Kits\\8.1\\include\\shared;c:\\Program Files (x86)\\Windows Kits\\8.1\\include\\um;c:\\Program Files (x86)\\Windows Kits\\8.1\\include\\winrt;c:\\Program Files (x86)\\Windows Kits\\8.1\\include\\winrt\\wrl;c:\\Program Files (x86)\\Windows Kits\\8.1\\include\\winrt\\wrl\\wrappers;{_VSPATH}\\vc\\include;{_VSPATH}\\vc\\atlmfc\\include;c:\\tools\\sdks\\dx10\\include'.format(_VSPATH=VSPATH),
       'LIB': 'c:\\Program Files (x86)\\Windows Kits\\8.1\\Lib\\winv6.3\\um\\x86;{_VSPATH}\\vc\\lib;{_VSPATH}\\vc\\atlmfc\\lib;c:\\tools\\sdks\\dx10\\lib'.format(_VSPATH=VSPATH),
       'LIBPATH': 'c:\\Program Files (x86)\\Windows Kits\\8.1\\Lib\\winv6.3\\um\\x86;{_VSPATH}\\vc\\lib;{_VSPATH}\\vc\\atlmfc\\lib;c:\\tools\\sdks\\dx10\\lib'.format(_VSPATH=VSPATH),
       'WINDOWSSDKDIR': 'c:\\Program Files (x86)\\Windows Kits\\8.1\\',
   },
}
