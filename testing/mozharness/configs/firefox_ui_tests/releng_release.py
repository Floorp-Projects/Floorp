# Default configuration as used by Release Engineering for testing release/beta builds

import os


config = {
    'env': {
        'PIP_TRUSTED_HOST': 'pypi.pub.build.mozilla.org',
    },

    # General local variable overwrite
    'exes': {
        'gittool.py': os.path.join(os.getcwd(), 'external_tools', 'gittool.py'),
        'hgtool.py': os.path.join(os.getcwd(), 'external_tools', 'hgtool.py'),
    },

    # PIP
    'find_links': ['http://pypi.pub.build.mozilla.org/pub'],
    'pip_index': False,

    # mozcrash support
    'download_minidump_stackwalk': True,
    'download_symbols': 'ondemand',
    'download_tooltool': True,
}
