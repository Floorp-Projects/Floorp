#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Given a list of packages and the versions to mirror,
generate a diff appropriate for mirroring
https://github.com/mozilla/mozbase
to http://hg.mozilla.org/mozilla-central/file/tip/testing/mozbase

If a package version is not given, the latest version will be used.

Note that this shells out to `cp` for simplicity, so you should run this
somewhere that has the `cp` command available.

Your mozilla-central repository must have no outstanding changes before this
script is run.  The repository must also have no untracked
files that show up in `hg st`.

See: https://bugzilla.mozilla.org/show_bug.cgi?id=702832
"""

from __future__ import with_statement

import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

from subprocess import check_call as call

# globals
here = os.path.dirname(os.path.abspath(__file__))
MOZBASE = 'git://github.com/mozilla/mozbase.git'
version_regex = r"""PACKAGE_VERSION *= *['"]([0-9.]+)["'].*"""

def error(msg):
    """err out with a message"""
    print >> sys.stdout, msg
    sys.exit(1)

def remove(path):
    """remove a file or directory"""
    if os.path.isdir(path):
        shutil.rmtree(path)
    else:
        os.remove(path)

### git functions

def latest_commit(git_dir):
    """returns last commit hash from a git repository directory"""
    command = ['git', 'log', '--pretty=format:%H',  'HEAD^..HEAD']
    process = subprocess.Popen(command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               cwd=git_dir)
    stdout, stderr = process.communicate()
    return stdout.strip()

def tags(git_dir):
    """return all tags in a git repository"""

    command = ['git', 'tag']
    process = subprocess.Popen(command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               cwd=git_dir)
    stdout, stderr = process.communicate()
    return [line.strip() for line in stdout.strip().splitlines()]

def checkout(git_dir, tag):
    """checkout a tagged version of a git repository"""

    command = ['git', 'checkout', tag]
    process = subprocess.Popen(command,
                               cwd=git_dir)
    process.communicate()

### hg functions

def untracked_files(hg_dir):
    """untracked files in an hg repository"""
    process = subprocess.Popen(['hg', 'st'],
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               cwd=hg_dir)
    stdout, stderr = process.communicate()
    lines = [line.strip() for line in stdout.strip().splitlines()]
    status = [line.split(None, 1) for line in lines]
    return [j for i, j in status if i == '?']

def revert(hg_dir, excludes=()):
    """revert a hg repository directory"""
    call(['hg', 'revert', '--no-backup', '--all'], cwd=hg_dir)
    newfiles = untracked_files(hg_dir)
    for f in newfiles:
        path = os.path.join(hg_dir, f)
        if path not in excludes:
            os.remove(path)


### version-related functions

def parse_versions(*args):
    """return a list of 2-tuples of (directory, version)"""

    retval = []
    for arg in args:
        if '=' in arg:
            directory, version = arg.split('=', 1)
        else:
            directory = arg
            version = None
        retval.append((directory, version))
    return retval

def version_tag(directory, version):
    return '%s-%s' % (directory, version)


###

def main(args=sys.argv[1:]):
    """command line entry point"""

    # parse command line options
    usage = '%prog [options] package1[=version1] <package2=version2> <...>'
    class PlainDescriptionFormatter(optparse.IndentedHelpFormatter):
        """description formatter for console script entry point"""
        def format_description(self, description):
            if description:
                return description.strip() + '\n'
            else:
                return ''
    parser = optparse.OptionParser(usage=usage,
                                   description=__doc__,
                                   formatter=PlainDescriptionFormatter())
    parser.add_option('-o', '--output', dest='output',
                      help="specify the output file; otherwise will be in the current directory with a name based on the hash")
    options, args = parser.parse_args(args)
    if args:
        versions = parse_versions(*args)
    else:
        parser.print_help()
        parser.exit()
    output = options.output

    # calculate hg root
    hg_root = os.path.dirname(os.path.dirname(here))  # testing/mozbase
    hg_dir = os.path.join(hg_root, '.hg')
    assert os.path.exists(hg_dir) and os.path.isdir(hg_dir)

    # ensure there are no outstanding changes to m-c
    process = subprocess.Popen(['hg', 'diff'], cwd=here, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if stdout.strip():
        error("Outstanding changes in %s; aborting" % hg_root)

    # ensure that there are no untracked files in testing/mozbase
    untracked = untracked_files(hg_root)
    if untracked:
        error("Untracked files in %s:\n %s\naborting" % (hg_root, '\n'.join([' %s' % i for i in untracked])))

    tempdir = tempfile.mkdtemp()
    try:

        # download mozbase
        call(['git', 'clone', MOZBASE], cwd=tempdir)
        src = os.path.join(tempdir, 'mozbase')
        assert os.path.isdir(src)
        if output is None:
            commit_hash = latest_commit(src)
            output = os.path.join(os.getcwd(), '%s.diff' % commit_hash)

        # get the tags
        _tags = tags(src)

        # ensure all directories and tags are available
        for index, (directory, version) in enumerate(versions):
            if not version:
                # choose maximum version from setup.py
                setup_py = os.path.join(src, directory, 'setup.py')
                assert os.path.exists(setup_py), "'%s' not found" % setup_py
                with file(setup_py) as f:
                    for line in f.readlines():
                        line = line.strip()
                        match = re.match(version_regex, line)
                        if match:
                            version = match.groups()[0]
                            versions[index] = (directory, version)
                            print "Using %s=%s" % (directory, version)
                            break
                    else:
                        error("Cannot find PACKAGE_VERSION in %s" % setup_py)

            tag = version_tag(directory, version)
            if tag not in _tags:
                error("Tag for '%s' -- %s -- not in tags")

        # copy mozbase directories to m-c
        for directory, version in versions:

            # checkout appropriate revision of mozbase
            tag = version_tag(directory, version)
            checkout(src, tag)

            # replace the directory
            remove(os.path.join(here, directory))
            call(['cp', '-r', directory, here], cwd=src)

        # generate the diff and write to output file
        call(['hg', 'addremove'], cwd=hg_root)
        process = subprocess.Popen(['hg', 'diff'],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   cwd=hg_root)
        stdout, stderr = process.communicate()
        with file(output, 'w') as f:
            f.write(stdout)
            f.close()

        # ensure that the diff you just wrote isn't deleted
        untracked.append(os.path.abspath(output))

    finally:
        # cleanup
        revert(hg_root, untracked)
        shutil.rmtree(tempdir)

    print "Diff at %s" % output

if __name__ == '__main__':
    main()
