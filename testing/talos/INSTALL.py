#!/usr/bin/env python

"""
installation script for talos. This script:
- creates a virtualenv in the current directory
- sets up talos in development mode: `python setup.py develop`
- downloads pageloader and packages to talos/page_load_test/pageloader.xpi
"""

import os
import subprocess
import sys
import urllib2
try:
    from subprocess import check_call as call
except:
    from subprocess import call

# globals
here = os.path.dirname(os.path.abspath(__file__))
VIRTUALENV = 'https://raw.github.com/pypa/virtualenv/1.10/virtualenv.py'


def which(binary, path=os.environ['PATH']):
    dirs = path.split(os.pathsep)
    for dir in dirs:
        if os.path.isfile(os.path.join(dir, path)):
            return os.path.join(dir, path)
        if os.path.isfile(os.path.join(dir, path + ".exe")):
            return os.path.join(dir, path + ".exe")


def main(args=sys.argv[1:]):

    # sanity check
    # ensure setup.py exists
    setup_py = os.path.join(here, 'setup.py')
    assert os.path.exists(setup_py), "setup.py not found"

    # create a virtualenv
    virtualenv = which('virtualenv') or which('virtualenv.py')
    if virtualenv:
        call([virtualenv, '--system-site-packages', here])
    else:
        process = subprocess.Popen([sys.executable,
                                    '-',
                                    '--system-site-packages',
                                    here],
                                   stdin=subprocess.PIPE)
        stdout, stderr = process.communicate(input=urllib2.urlopen(VIRTUALENV).read())

    # find the virtualenv's python
    for i in ('bin', 'Scripts'):
        bindir = os.path.join(here, i)
        if os.path.exists(bindir):
            break
    else:
        raise AssertionError('virtualenv binary directory not found')
    for i in ('python', 'python.exe'):
        virtualenv_python = os.path.join(bindir, i)
        if os.path.exists(virtualenv_python):
            break
    else:
        raise AssertionError('virtualenv python not found')

    # install talos into the virtualenv
    call([os.path.abspath(virtualenv_python), 'setup.py', 'develop'], cwd=here)


if __name__ == '__main__':
    main()
