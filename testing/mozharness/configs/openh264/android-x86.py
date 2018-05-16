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
        'upload',
    ],

    'tooltool_manifest_file': "android.manifest",
    'tooltool_cache': "/builds/tooltool_cache",
    'exes': {
        'gittool.py': [os.path.join(external_tools_path, 'gittool.py')],
        'python2.7': "/tools/python27/bin/python2.7",
    },
    'avoid_avx2': True,
    'dump_syms_binary': 'dump_syms',
    'arch': 'x86',
    # https://dxr.mozilla.org/mozilla-central/rev/5322c03f4c8587fe526172d3f87160031faa6d75/mobile/android/config/mozconfigs/android-x86/nightly#4
    'min_sdk': 16,
    'operating_system': 'android',
    'partial_env': {
        'PATH': '%(abs_work_dir)s/android-sdk-linux/tools:%(PATH)s',
    },
}
