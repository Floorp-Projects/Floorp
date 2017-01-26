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
    # PIP
    'find_links': ['http://pypi.pub.build.mozilla.org/pub'],
    'pip_index': False,

    # mozcrash support
    'download_minidump_stackwalk': True,
    'download_symbols': 'ondemand',
    'download_tooltool': True,

    'default_actions': [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-media-tests',
    ],

}

