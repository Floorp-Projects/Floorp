#!/usr/bin/env python

"""
generate a diff appropriate for mirroring
https://github.com/mozilla/mozbase
to http://hg.mozilla.org/mozilla-central/file/tip/testing/mozbase

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
import shutil
import subprocess
import sys
import tempfile

from subprocess import check_call as call

# globals
here = os.path.dirname(os.path.abspath(__file__))
MOZBASE = 'git://github.com/mozilla/mozbase.git'

# paths we don't want in m-c from mozbase's github repo
git_excludes = ['.git',
                '.gitignore',
                'versionbump.py']

# top-level paths we want to keep in m-c that aren't in the github repo
keep = ['Makefile.in',
        'generate_diff.py']

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

def latest_commit(git_dir):
    """returns last commit hash from a git repository directory"""
    command = ['git', 'log', '--pretty=format:%H',  'HEAD^..HEAD']
    process = subprocess.Popen(command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               cwd=git_dir)
    stdout, stderr = process.communicate()
    return stdout.strip()

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

def main(args=sys.argv[1:]):
    """command line entry point"""

    # parse command line options
    usage = '%prog output'
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
    options, args = parser.parse_args(args)
    if len(args) > 1:
        parser.print_help()
        parser.exit()
    if args:
        output = args[0]
    else:
        output = None

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

        # remove files from github clone we don't want to copy
        for path in git_excludes:
            path = os.path.join(src, path)
            if not os.path.exists(path):
                continue
            remove(path)

        # remove the files from m-c we don't want to keep
        for path in os.listdir(here):
            if path in keep:
                continue
            path = os.path.join(here, path)
            remove(path)

        # copy mozbase to m-c
        for path in os.listdir(src):
            call(['cp', '-r', path, here], cwd=src)

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
