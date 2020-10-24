#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Setup mozbase packages for development.

Packages may be specified as command line arguments.
If no arguments are given, install all packages.

See https://wiki.mozilla.org/Auto-tools/Projects/Mozbase
"""

from __future__ import absolute_import, print_function

import os
import subprocess
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
mozbase_packages = [i for i in os.listdir(here)
                    if os.path.exists(os.path.join(here, i, 'setup.py'))]

# testing: https://wiki.mozilla.org/Auto-tools/Projects/Mozbase#Tests
test_packages = ["mock"]

# documentation: https://wiki.mozilla.org/Auto-tools/Projects/Mozbase#Documentation
extra_packages = ["sphinx"]


def cycle_check(order, dependencies):
    """ensure no cyclic dependencies"""
    order_dict = dict([(j, i) for i, j in enumerate(order)])
    for package, deps in dependencies.items():
        index = order_dict[package]
        for d in deps:
            assert index > order_dict[d], "Cyclic dependencies detected"


def info(directory):
    "get the package setup.py information"

    assert os.path.exists(os.path.join(directory, 'setup.py'))

    # setup the egg info
    try:
        call([sys.executable, 'setup.py', 'egg_info'],
             cwd=directory, stdout=PIPE)
    except subprocess.CalledProcessError:
        print("Error running setup.py in %s" % directory)
        raise

    # get the .egg-info directory
    egg_info = [entry for entry in os.listdir(directory)
                if entry.endswith('.egg-info')]
    assert len(egg_info) == 1, 'Expected one .egg-info directory in %s, got: %s' % (directory,
                                                                                    egg_info)
    egg_info = os.path.join(directory, egg_info[0])
    assert os.path.isdir(egg_info), "%s is not a directory" % egg_info

    # read the package information
    pkg_info = os.path.join(egg_info, 'PKG-INFO')
    info_dict = {}
    for line in open(pkg_info).readlines():
        if not line or line[0].isspace():
            continue  # XXX neglects description
        assert ':' in line
        key, value = [i.strip() for i in line.split(':', 1)]
        info_dict[key] = value

    return info_dict


def get_dependencies(directory):
    "returns the package name and dependencies given a package directory"

    # get the package metadata
    info_dict = info(directory)

    # get the .egg-info directory
    egg_info = [entry for entry in os.listdir(directory)
                if entry.endswith('.egg-info')][0]

    # read the dependencies
    requires = os.path.join(directory, egg_info, 'requires.txt')
    dependencies = []
    if os.path.exists(requires):
        for line in open(requires):
            line = line.strip()
            # in requires.txt file, a dependency is a non empty line
            # Also lines like [device] are sections to mark optional
            # dependencies, we don't want those sections.
            if line and not (line.startswith('[') and line.endswith(']')):
                dependencies.append(line)

    # return the information
    return info_dict['Name'], dependencies


def dependency_info(dep):
    "return dictionary of dependency information from a dependency string"
    retval = dict(Name=None, Type=None, Version=None)
    for joiner in ('==', '<=', '>='):
        if joiner in dep:
            retval['Type'] = joiner
            name, version = [i.strip() for i in dep.split(joiner, 1)]
            retval['Name'] = name
            retval['Version'] = version
            break
    else:
        retval['Name'] = dep.strip()
    return retval


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

    cycle_check(order, dependencies)  # sanity check

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
    parser.add_option('--extra', '--install-extra-packages', action='store_true', default=False,
                      help="installs extra supporting packages as well as core mozbase ones")
    options, packages = parser.parse_args(args)

    if not packages:
        # install all packages
        packages = sorted(mozbase_packages)

    # ensure specified packages are in the list
    assert set(packages).issubset(mozbase_packages), \
        "Packages should be in %s (You gave: %s)" % (mozbase_packages, packages)

    if options.list_dependencies:
        # list the package dependencies
        for package in packages:
            print('%s: %s' % get_dependencies(os.path.join(here, package)))
        parser.exit()

    # gather dependencies
    # TODO: version conflict checking
    deps = {}
    alldeps = {}
    mapping = {}  # mapping from subdir name to package name
    # core dependencies
    for package in packages:
        key, value = get_dependencies(os.path.join(here, package))
        deps[key] = [dependency_info(dep)['Name'] for dep in value]
        mapping[package] = key

        # keep track of all dependencies for non-mozbase packages
        for dep in value:
            alldeps[dependency_info(dep)['Name']] = ''.join(dep.split())

    # indirect dependencies
    flag = True
    while flag:
        flag = False
        for value in deps.values():
            for dep in value:
                if dep in mozbase_packages and dep not in deps:
                    key, value = get_dependencies(os.path.join(here, dep))
                    deps[key] = [dep for dep in value]

                    for dep in value:
                        alldeps[dep] = ''.join(dep.split())
                    mapping[package] = key
                    flag = True
                    break
            if flag:
                break

    # get the remaining names for the mapping
    for package in mozbase_packages:
        if package in mapping:
            continue
        key, value = get_dependencies(os.path.join(here, package))
        mapping[package] = key

    # unroll dependencies
    unrolled = unroll_dependencies(deps)

    # make a reverse mapping: package name -> subdirectory
    reverse_mapping = dict([(j, i) for i, j in mapping.items()])

    # we only care about dependencies in mozbase
    unrolled = [package for package in unrolled if package in reverse_mapping]

    if options.list:
        # list what will be installed
        for package in unrolled:
            print(package)
        parser.exit()

    # set up the packages for development
    for package in unrolled:
        call([sys.executable, 'setup.py', 'develop', '--no-deps'],
             cwd=os.path.join(here, reverse_mapping[package]))

    # add the directory of sys.executable to path to aid the correct
    # `easy_install` getting called
    # https://bugzilla.mozilla.org/show_bug.cgi?id=893878
    os.environ['PATH'] = '%s%s%s' % (os.path.dirname(os.path.abspath(sys.executable)),
                                     os.path.pathsep,
                                     os.environ.get('PATH', '').strip(os.path.pathsep))

    # install non-mozbase dependencies
    # these need to be installed separately and the --no-deps flag
    # subsequently used due to a bug in setuptools; see
    # https://bugzilla.mozilla.org/show_bug.cgi?id=759836
    pypi_deps = dict([(i, j) for i, j in alldeps.items()
                      if i not in unrolled])
    for package, version in pypi_deps.items():
        # easy_install should be available since we rely on setuptools
        call(['easy_install', version])

    # install packages required for unit testing
    for package in test_packages:
        call(['easy_install', package])

    # install extra non-mozbase packages if desired
    if options.extra:
        for package in extra_packages:
            call(['easy_install', package])


if __name__ == '__main__':
    main()
