import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    'tooltool_manifest_file': "osx.manifest",
    'tooltool_cache': "/builds/tooltool_cache",
    'exes': {
        'gittool.py': [os.path.join(external_tools_path, 'gittool.py')],
        'tooltool.py': "/builds/tooltool.py",
        'python2.7': "/tools/python27/bin/python2.7",
    },
    'dump_syms_binary': 'dump_syms',
    'arch': 'x64',
    'use_yasm': True,
}
