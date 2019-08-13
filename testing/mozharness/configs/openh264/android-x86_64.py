import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    'default_actions': [
        'get-tooltool',
        'checkout-sources',
        'build',
        # 'test',  # can't run android tests on linux hosts
        'package',
        'dump-symbols',
    ],

    'tooltool_manifest_file': "android.manifest",
    'tooltool_cache': "/builds/tooltool_cache",
    'exes': {
        'gittool.py': [os.path.join(external_tools_path, 'gittool.py')],
        'python2.7': "python2.7",
    },
    'avoid_avx2': True,
    'dump_syms_binary': 'dump_syms',
    'arch': 'x86_64',
    'min_sdk': 16,
    'operating_system': 'android',
    'partial_env': {
        'CXXFLAGS': '-stdlib=libstdc++',
        'LDFLAGS': '-stdlib=libstdc++',
        'PATH': ('{MOZ_FETCHES_DIR}/android-sdk-linux/tools:'
                 '{MOZ_FETCHES_DIR}/clang/bin:'
                 '{MOZ_FETCHES_DIR}/nasm:%(PATH)s'
                 .format(MOZ_FETCHES_DIR=os.environ['MOZ_FETCHES_DIR'])),
    },
}
