# This config file has generic values needed for any job and any platform running
# on Release Engineering machines inside the VPN
from mozharness.base.script import platform_name

# These are values specific to each platform on Release Engineering machines
PYTHON_WIN32 = 'c:/mozilla-build/python27/python.exe'
# These are values specific to running machines on Release Engineering machines
# to run it locally on your machines append --cfg developer_config.py
PLATFORM_CONFIG = {
    'linux64': {
        'exes': {
            'gittool.py': '/usr/local/bin/gittool.py',
        },
        'env': {
            'DISPLAY': ':2',
        }
    },
    'macosx': {
        'exes': {
            'gittool.py': '/usr/local/bin/gittool.py',
        },
    },
    'win32': {
        "exes": {
            'gittool.py': [PYTHON_WIN32, 'c:/builds/hg-shared/build/tools/buildfarm/utils/gittool.py'],
            # Otherwise, depending on the PATH we can pick python 2.6 up
            'python': PYTHON_WIN32,
        }
    }
}

config = PLATFORM_CONFIG[platform_name()]
# Generic values
config.update({
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    'pip_index': False,
    'virtualenv_path': 'venv',
})

