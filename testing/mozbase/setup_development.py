#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozbase.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jeff Hammel <jhammel@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""
Setup mozbase packages for development.

Packages may be specified as command line arguments.
If no arguments are given, install all packages.

See https://wiki.mozilla.org/Auto-tools/Projects/MozBase
"""

# XXX note that currently directory names must equal package names

import pkg_resources
import os
import sys
from optparse import OptionParser

from subprocess import PIPE
try:
    from subprocess import check_call as call
except ImportError:
    from subprocess import call


# directory containing this file
here = os.path.dirname(os.path.abspath(__file__))

# all python packages
all_packages = [i for i in os.listdir(here)
                if os.path.exists(os.path.join(here, i, 'setup.py'))]

def cycle_check(order, dependencies):
    """ensure no cyclic dependencies"""
    order_dict = dict([(j, i) for i, j in enumerate(order)])
    for package, deps in dependencies.items():
        index = order_dict[package]
        for d in deps:
            assert index > order_dict[d], "Cyclic dependencies detected"

def dependencies(directory):
    """
    get the dependencies of a package directory containing a setup.py
    returns the package name and the list of dependencies
    """
    assert os.path.exists(os.path.join(directory, 'setup.py'))

    # setup the egg info
    call([sys.executable, 'setup.py', 'egg_info'], cwd=directory, stdout=PIPE)

    # get the .egg-info directory
    egg_info = [i for i in os.listdir(directory)
                if i.endswith('.egg-info')]
    assert len(egg_info) == 1, 'Expected one .egg-info directory in %s, got: %s' % (directory, egg_info)
    egg_info = os.path.join(directory, egg_info[0])
    assert os.path.isdir(egg_info), "%s is not a directory" % egg_info

    # read the dependencies
    requires = os.path.join(egg_info, 'requires.txt')
    if os.path.exists(requires):
        dependencies = [i.strip() for i in file(requires).readlines() if i.strip()]
    else:
        dependencies = []

    # read the package information
    pkg_info = os.path.join(egg_info, 'PKG-INFO')
    info_dict = {}
    for line in file(pkg_info).readlines():
        if not line or line[0].isspace():
            continue # XXX neglects description
        assert ':' in line
        key, value = [i.strip() for i in line.split(':', 1)]
        info_dict[key] = value


    # return the information
    return info_dict['Name'], dependencies

def sanitize_dependency(dep):
    """
    remove version numbers from deps
    """
    for joiner in ('==', '<=', '>='):
        if joiner in dep:
            dep = dep.split(joiner, 1)[0].strip()
            return dep # XXX only one joiner allowed right now
    return dep


def unroll_dependencies(dependencies):
    """
    unroll a set of dependencies to a flat list

    dependencies = {'packageA': set(['packageB', 'packageC', 'packageF']),
                    'packageB': set(['packageC', 'packageD', 'packageE', 'packageG']),
                    'packageC': set(['packageE']),
                    'packageE': set(['packageF', 'packageG']),
                    'packageF': set(['packageG']),
                    'packageX': set(['packageA', 'packageG'])}
    """

    order = []

    # flatten all
    packages = set(dependencies.keys())
    for deps in dependencies.values():
        packages.update(deps)

    while len(order) != len(packages):

        for package in packages.difference(order):
            if set(dependencies.get(package, set())).issubset(order):
                order.append(package)
                break
        else:
            raise AssertionError("Cyclic dependencies detected")

    cycle_check(order, dependencies) # sanity check

    return order


def main(args=sys.argv[1:]):

    # parse command line options
    usage = '%prog [options] [package] [package] [...]'
    parser = OptionParser(usage=usage, description=__doc__)
    parser.add_option('-d', '--dependencies', dest='list_dependencies',
                      action='store_true', default=False,
                      help="list dependencies for the packages")
    parser.add_option('--list', action='store_true', default=False,
                      help="list what will be installed")
    options, packages = parser.parse_args(args)

    if not packages:
        # install all packages
        packages = sorted(all_packages)

    # ensure specified packages are in the list
    assert set(packages).issubset(all_packages), "Packages should be in %s (You gave: %s)" % (all_packages, packages)

    if options.list_dependencies:
        # list the package dependencies
        for package in packages:
            print '%s: %s' % dependencies(os.path.join(here, package))
        parser.exit()

    # gather dependencies
    deps = {}
    mapping = {} # mapping from subdir name to package name
    # core dependencies
    for package in packages:
        key, value = dependencies(os.path.join(here, package))
        deps[key] = [sanitize_dependency(dep) for dep in value]
        mapping[package] = key
    # indirect dependencies
    flag = True
    while flag:
        flag = False
        for value in deps.values():
            for dep in value:
                if dep in all_packages and dep not in deps:
                    key, value = dependencies(os.path.join(here, dep))
                    deps[key] = [sanitize_dependency(dep) for dep in value]
                    mapping[package] = key
                    flag = True
                    break
            if flag:
                break

    # get the remaining names for the mapping
    for package in all_packages:
        if package in mapping:
            continue
        key, value = dependencies(os.path.join(here, package))
        mapping[package] = key

    # unroll dependencies
    unrolled = unroll_dependencies(deps)

    # make a reverse mapping: package name -> subdirectory
    reverse_mapping = dict([(j,i) for i, j in mapping.items()])

    # we only care about dependencies in mozbase
    unrolled = [package for package in unrolled if package in reverse_mapping]

    if options.list:
        # list what will be installed
        for package in unrolled:
            print package
        parser.exit()

    # set up the packages for development
    for package in unrolled:
        call([sys.executable, 'setup.py', 'develop'],
             cwd=os.path.join(here, reverse_mapping[package]))

if __name__ == '__main__':
    main()
