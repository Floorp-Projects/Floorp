#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
install firebug and its dependencies
"""

import os
import sys
from optparse import OptionParser
from subprocess import call

### utility functions for cross-platform

def is_windows():
    return sys.platform.startswith('win')

def esc(path):
    """quote and escape a path for cross-platform use"""
    return '"%s"' % repr(path)[1:-1]

def scripts_path(virtual_env):
    """path to scripts directory"""
    if is_windows():
        return os.path.join(virtual_env, 'Scripts')
    return os.path.join(virtual_env, 'bin')

def python_script_path(virtual_env, script_name):
    """path to a python script in a virtualenv"""
    scripts_dir = scripts_path(virtual_env)
    if is_windows():
        script_name = script_name + '-script.py'
    return os.path.join(scripts_dir, script_name)

def entry_point_path(virtual_env, entry_point):
    path = os.path.join(scripts_path(virtual_env), entry_point)
    if is_windows():
        path += '.exe'
    return path

### command-line entry point

def main(args=None):
    """command line front-end function"""

    # parse command line arguments
    args = args or sys.argv[1:]
    usage = "Usage: %prog [options] [destination]"
    parser = OptionParser(usage=usage)
    parser.add_option('--develop', dest='develop',
                      action='store_true', default=False,
                      help='setup in development mode')
    options, args = parser.parse_args(args)

    # Print the python version
    print 'Python: %s' % sys.version

    # The data is kept in the same directory as the script
    source=os.path.abspath(os.path.dirname(__file__))

    # directory to install to
    if not len(args):
        destination = source
    elif len(args) == 1:
        destination = os.path.abspath(args[0])
    else:
        parser.print_usage()
        parser.exit(1)

    os.chdir(source)

    # check for existence of necessary files
    if not os.path.exists('virtualenv'):
        print "File not found: virtualenv"
        sys.exit(1)
    PACKAGES_FILE = 'PACKAGES'
    if not os.path.exists(PACKAGES_FILE) and destination != source:
        PACKAGES_FILE = os.path.join(destination, PACKAGES_FILE)
    if not os.path.exists(PACKAGES_FILE):
        print "File not found: PACKAGES"

    # packages to install in dependency order
    PACKAGES=file(PACKAGES_FILE).read().split()
    assert PACKAGES
  
    # create the virtualenv and install packages
    env = os.environ.copy()
    env.pop('PYTHONHOME', None)
    returncode = call([sys.executable, os.path.join('virtualenv', 'virtualenv.py'), destination], env=env)
    if returncode:
        print 'Failure to install virtualenv'
        sys.exit(returncode)
    if options.develop:
        python = entry_point_path(destination, 'python')
        for package in PACKAGES:
            oldcwd = os.getcwd()
            os.chdir(package)
            returncode = call([python, 'setup.py', 'develop'])
            os.chdir(oldcwd)
            if returncode:
                break
    else:
        pip = entry_point_path(destination, 'pip')
        returncode = call([pip, 'install'] + PACKAGES, env=env)

    if returncode:
        print 'Failure to install packages'
        sys.exit(returncode)

if __name__ == '__main__':
    main()
