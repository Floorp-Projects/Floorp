import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    'tooltool_manifest_file': "linux.manifest",
    'tooltool_cache': "/builds/tooltool_cache",
    'exes': {
        'gittool.py': [os.path.join(external_tools_path, 'gittool.py')],
        'tooltool.py': "/builds/tooltool.py",
        'python2.7': "/tools/python27/bin/python2.7",
    },
    'dump_syms_binary': 'dump_syms',
    'arch': 'x64',
    'use_mock': True,
    'avoid_avx2': True,
    'mock_target': 'mozilla-centos6-x86_64',
    'mock_packages': ['make', 'git', 'nasm', 'glibc-devel.i686',
                      'libstdc++-devel.i686', 'zip', 'yasm',
                      'mozilla-python27'],
    'mock_files': [
        ('/home/cltbld/.ssh', '/home/mock_mozilla/.ssh'),
        ('/builds/relengapi.tok', '/builds/relengapi.tok'),
        ('/tools/tooltool.py', '/builds/tooltool.py'),
    ],
    'operating_system': 'linux',
}
