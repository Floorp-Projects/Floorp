# Default configuration as used by Mozmill CI (Jenkins)

import os
import platform
import sys

import mozharness


external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    'env': {
        'PIP_TRUSTED_HOST': 'pypi.pub.build.mozilla.org',
    },

    # PIP
    'find_links': ['http://pypi.pub.build.mozilla.org/pub'],
    'pip_index': False,

    # mozcrash support
    'download_minidump_stackwalk': True,
    'download_symbols': 'ondemand',
    'download_tooltool': True,

    # Version control information
    'firefox_media_repo': 'https://github.com/mjzffr/firefox-media-tests.git',
    'firefox_media_branch': 'master',

    # Default test suite
    'test_suite': 'media-tests',

    'suite_definitions': {
        'media-tests': {
            'options': [],
        },
        'media-youtube-tests': {
            'options': [
                '%(test_manifest)s'
            ],
        },
    },

    'default_actions': [
        'clobber',
        'checkout',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-media-tests',
    ],

}

# General local variable overwrite
# Bug 1227079 - Python executable eeded to get it executed on Windows
if platform.system() == 'windows':
    gittool = [
        sys.executable,
        os.path.join(external_tools_path, 'gittool.py')
    ]
else:
    gittool = os.path.join(external_tools_path, 'gittool.py')

exes = {
    'gittool.py' : gittool,
}

config['exes'] = exes
