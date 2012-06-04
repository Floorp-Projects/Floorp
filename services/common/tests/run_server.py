#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from argparse import ArgumentParser
from shutil import rmtree
from subprocess import Popen
from sys import argv
from sys import exit
from tempfile import mkdtemp

DEFAULT_PORT = 8080
DEFAULT_HOSTNAME = 'localhost'

def run_server(srcdir, objdir, js_file, hostname=DEFAULT_HOSTNAME,
        port=DEFAULT_PORT):

    dist_dir = '%s/dist' % objdir
    head_dir = '%s/services/common/tests/unit' % srcdir

    head_paths = [
        'head_global.js',
        'head_helpers.js',
        'head_http.js',
    ]

    head_paths = ['"%s/%s"' % (head_dir, path) for path in head_paths]

    args = [
        '%s/bin/xpcshell' % dist_dir,
        '-g', '%s/bin' % dist_dir,
        '-a', '%s/bin' % dist_dir,
        '-r', '%s/bin/components/httpd.manifest' % dist_dir,
        '-m',
        '-n',
        '-s',
        '-f', '%s/testing/xpcshell/head.js' % srcdir,
        '-e', 'const _SERVER_ADDR = "%s";' % hostname,
        '-e', 'const _TESTING_MODULES_DIR = "%s/_tests/modules";' % objdir,
        '-e', 'const SERVER_PORT = "%s";' % port,
        '-e', 'const INCLUDE_FILES = [%s];' % ', '.join(head_paths),
        '-e', '_register_protocol_handlers();',
        '-e', 'for each (let name in INCLUDE_FILES) load(name);',
        '-e', '_fakeIdleService.activate();',
        '-f', '%s/services/common/tests/%s' % (srcdir, js_file)
    ]

    profile_dir = mkdtemp()
    print 'Created profile directory: %s' % profile_dir

    try:
        env = {'XPCSHELL_TEST_PROFILE_DIR': profile_dir}
        proc = Popen(args, env=env)

        return proc.wait()

    finally:
        print 'Removing profile directory %s' % profile_dir
        rmtree(profile_dir)

if __name__ == '__main__':
    parser = ArgumentParser(description="Run a standalone JS server.")
    parser.add_argument('srcdir',
        help="Root directory of Firefox source code.")
    parser.add_argument('objdir',
        help="Root directory object directory created during build.")
    parser.add_argument('js_file',
        help="JS file (in this directory) to execute.")
    parser.add_argument('--port', default=DEFAULT_PORT, type=int,
        help="Port to run server on.")
    parser.add_argument('--address', default=DEFAULT_HOSTNAME,
        help="Hostname to bind server to.")

    args = parser.parse_args()

    exit(run_server(args.srcdir, args.objdir, args.js_file, args.address,
        args.port))
