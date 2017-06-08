#!/usr/bin/env python
"""
Run flake8 checks and tests.
"""

import os
import argparse
import pipes
import shutil
import tempfile

from subprocess import check_call


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument('-C', '--with-coverage', action='store_true',
                        help="Generate coverage data from the tests run")
    parser.add_argument('-H', '--cover-html', action='store_true',
                        help='generate html files to see test coverage')
    return parser.parse_args()


def run(cmd, **kwargs):
    msg = 'Running: |%s|' % ' '.join(pipes.quote(c) for c in cmd)
    if kwargs.get('cwd'):
        msg += ' in %s' % kwargs['cwd']
    print(msg)
    check_call(cmd, **kwargs)


def rm(path):
    if os.path.isfile(path):
        os.unlink(path)
    elif os.path.isdir(path):
        shutil.rmtree(path)


if __name__ == '__main__':
    options = parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    os.chdir(here)

    run(['flake8', 'dlmanager', 'tests', 'setup.py', __file__])

    if options.with_coverage:
        rm('.coverage')
        test_run_cmd = ['coverage', 'run']
    else:
        test_run_cmd = ['python']

    tmpdir = tempfile.gettempdir()
    tmpfiles = set(os.listdir(tmpdir))
    run(test_run_cmd + ['setup.py', 'test'])

    remaining_tmpfiles = tmpfiles - set(os.listdir(tmpdir))
    assert not remaining_tmpfiles, "tests leaked some temp files: %s" % (
        ", ".join("`%s`" % os.path.join(tmpdir, f) for f in remaining_tmpfiles)
    )

    if options.with_coverage and options.cover_html:
        rm('htmlcov')
        run(['coverage', 'html'])
        print("See coverage: |firefox %s|"
              % os.path.join(here, 'htmlcov', 'index.html'))
