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
        'python2.7': "python2.7",
    },
    'dump_syms_binary': 'dump_syms',
    'arch': 'x64',
    'avoid_avx2': True,
    'operating_system': 'linux',
    'partial_env': {
        'PATH': ('{MOZ_FETCHES_DIR}/clang/bin:'
                 '{MOZ_FETCHES_DIR}/binutils/bin:'
                 '{MOZ_FETCHES_DIR}/nasm:%(PATH)s'
                 .format(MOZ_FETCHES_DIR=os.environ['MOZ_FETCHES_DIR'])),
    },
}
