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
        'python2.7': "python2.7",
    },
    'dump_syms_binary': 'dump_syms',
    'arch': 'x64',
    'use_yasm': True,
    'operating_system': 'darwin',
    'partial_env': {
        'CXXFLAGS': ('-target x86_64-apple-darwin '
                     '-B {MOZ_FETCHES_DIR}/cctools/bin '
                     '-isysroot %(abs_work_dir)s/src/MacOSX10.11.sdk '
                     '-mmacosx-version-min=10.11'
                     .format(MOZ_FETCHES_DIR=os.environ['MOZ_FETCHES_DIR'])),
        'LDFLAGS': ('-target x86_64-apple-darwin '
                    '-B {MOZ_FETCHES_DIR}/cctools/bin '
                    '-isysroot %(abs_work_dir)s/src/MacOSX10.11.sdk '
                    '-mmacosx-version-min=10.11'
                     .format(MOZ_FETCHES_DIR=os.environ['MOZ_FETCHES_DIR'])),
        'PATH': ('{MOZ_FETCHES_DIR}/clang/bin/:%(PATH)s'
                 .format(MOZ_FETCHES_DIR=os.environ['MOZ_FETCHES_DIR'])),
    },
    "tooltool_servers": ['http://taskcluster/tooltool.mozilla-releng.net/'],
    "tooltool_url": 'http://taskcluster/tooltool.mozilla-releng.net/',
}
