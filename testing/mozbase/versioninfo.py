#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
List mozbase package dependencies or generate changelogs
from commit messages.
"""

from collections import Iterable
from distutils.version import StrictVersion
import argparse
import os
import subprocess
import sys

import setup_development

here = os.path.abspath(os.path.dirname(__file__))

def run_hg(command):
    command = command[:]
    if not isinstance(command, Iterable):
        command = command.split()
    command.insert(0, 'hg')
    try:
        output = subprocess.check_output(command, cwd=here)
    except subprocess.CalledProcessError:
        sys.exit(1)
    return output


def changelog(args):
    setup = os.path.join(args.module, 'setup.py')

    def get_version_rev(v=None):
        revisions = run_hg(['log', setup, '--template={rev},']).split(',')[:-1]
        for rev in revisions:
            diff = run_hg(['diff', '-c', rev, setup, '-U0'])
            minus_version = None
            plus_version = None
            for line in diff.splitlines():
                if line.startswith('-PACKAGE_VERSION'):
                    minus_version = StrictVersion(line.split()[-1].strip('"\''))
                elif line.startswith('+PACKAGE_VERSION'):
                    plus_version = StrictVersion(line.split()[-1].strip('"\''))

                    # make sure the change isn't a backout
                    if not minus_version or plus_version > minus_version:
                        if not v:
                            return rev

                        if StrictVersion(v) == plus_version:
                            return rev
        print("Could not find %s revision for version %s." % (args.module, v or 'latest'))
        sys.exit(1)

    from_ref = args.from_ref or get_version_rev()
    to_ref = args.to_ref or 'tip'

    if '.' in from_ref:
        from_ref = get_version_rev(from_ref)
    if '.' in to_ref:
        to_ref = get_version_rev(to_ref)

    delim = '\x12\x59\x52\x99\x05'
    changelog = run_hg(['log', '-r', '%s:children(%s)' % (to_ref, from_ref), '--template={desc}%s' % delim, '-M', args.module]).split(delim)[:-1]

    def prettify(desc):
        lines = desc.splitlines()
        lines = [('* %s' if i == 0 else '  %s') % l for i, l in enumerate(lines)]
        return '\n'.join(lines)

    changelog = map(prettify, changelog)
    print '\n'.join(changelog)


def dependencies(args):
    # get package information
    info = {}
    dependencies = {}
    for package in setup_development.mozbase_packages:
        directory = os.path.join(setup_development.here, package)
        info[directory] = setup_development.info(directory)
        name, _dependencies = setup_development.get_dependencies(directory)
        assert name == info[directory]['Name']
        dependencies[name] = _dependencies

    # print package version information
    for value in info.values():
        print '%s %s : %s' % (value['Name'], value['Version'],
                              ', '.join(dependencies[value['Name']]))


def main(args=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    subcommands = parser.add_subparsers(help="Sub-commands")

    p_deps = subcommands.add_parser('dependencies', help="Print dependencies.")
    p_deps.set_defaults(func=dependencies)

    p_changelog = subcommands.add_parser('changelog', help="Print a changelog.")
    p_changelog.add_argument('module', help="Module to get changelog from.")
    p_changelog.add_argument('--from', dest='from_ref', default=None,
                             help="Starting version or revision to list changes from. [defaults to latest version]")
    p_changelog.add_argument('--to', dest='to_ref', default=None,
                             help="Ending version or revision to list changes to. [defaults to tip]")
    p_changelog.set_defaults(func=changelog)

    # default to showing dependencies
    if args == []:
        args.append('dependencies')
    args = parser.parse_args(args)
    args.func(args)


if __name__ == '__main__':
    main()
