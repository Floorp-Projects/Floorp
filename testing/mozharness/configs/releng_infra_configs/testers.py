# This config file has generic values needed for any job and any platform running
# on Release Engineering machines inside the VPN
import os

import mozharness

from mozharness.base.script import platform_name

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

# These are values specific to each platform on Release Engineering machines
PYTHON_WIN32 = 'c:/mozilla-build/python27/python.exe'
# These are values specific to running machines on Release Engineering machines
# to run it locally on your machines append --cfg developer_config.py
PLATFORM_CONFIG = {
    'linux': {
        'exes': {
            'gittool.py': os.path.join(external_tools_path, 'gittool.py'),
        },
        'env': {
            'DISPLAY': ':0',
            'PATH': '%(PATH)s:' + external_tools_path,
        }
    },
    'linux64': {
        'exes': {
            'gittool.py': os.path.join(external_tools_path, 'gittool.py'),
        },
        'env': {
            'DISPLAY': ':0',
            'PATH': '%(PATH)s:' + external_tools_path,
        }
    },
    'macosx': {
        'exes': {
            'gittool.py': os.path.join(external_tools_path, 'gittool.py'),
        },
        'env': {
            'PATH': '%(PATH)s:' + external_tools_path,
        }
    },
    'win32': {
        "exes": {
            'gittool.py': [PYTHON_WIN32, os.path.join(external_tools_path, 'gittool.py')],
            # Otherwise, depending on the PATH we can pick python 2.6 up
            'python': PYTHON_WIN32,
        }
    }
}

config = PLATFORM_CONFIG[platform_name()]
# Generic values
config.update({
    'virtualenv_path': 'venv',
})
