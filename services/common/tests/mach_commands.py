# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import mozpack.path
import os
import sys
import warnings
import which

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mach.registrar import (
    Registrar
)

from shutil import rmtree
from subprocess import Popen
from sys import argv
from sys import exit
from tempfile import mkdtemp

import mozpack.path as mozpath


DEFAULT_PORT = 8080
DEFAULT_HOSTNAME = 'localhost'

SRCDIR = mozpath.abspath(mozpath.dirname(__file__))

STORAGE_SERVER_SCRIPT = mozpath.join(SRCDIR, 'run_storage_server.js')
BAGHEERA_SERVER_SCRIPT = mozpath.join(SRCDIR, 'run_bagheera_server.js')

def SyncStorageCommand(func):
    """Decorator that adds shared command arguments to services commands."""

    port = CommandArgument('--port', metavar='PORT', type=int,
                           default=DEFAULT_PORT, help='Port to run server on.')
    func = port(func)

    address = CommandArgument('--address', metavar='ADDRESS',
                              default=DEFAULT_HOSTNAME,
                              help='Hostname to bind server to.')
    func = address(func)

    return func

Registrar.register_category(name='services',
                            title='Services utilities',
                            description='Commands for services development.')

@CommandProvider
class SyncTestCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

    def run_server(self, js_file, hostname, port):
        topsrcdir = self.topsrcdir
        topobjdir = self.topobjdir

        unit_test_dir = mozpath.join(SRCDIR, 'unit')

        head_paths = [
            'head_global.js',
            'head_helpers.js',
            'head_http.js',
            ]

        head_paths = ['"%s"' % mozpath.join(unit_test_dir, path) for path in head_paths]

        args = [
            '%s/run-mozilla.sh' % self.bindir,
            '%s/xpcshell' % self.bindir,
            '-g', self.bindir,
            '-a', self.bindir,
            '-r', '%s/components/httpd.manifest' % self.bindir,
            '-m',
            '-s',
            '-f', '%s/testing/xpcshell/head.js' % topsrcdir,
            '-e', 'const _SERVER_ADDR = "%s";' % hostname,
            '-e', 'const _TESTING_MODULES_DIR = "%s/_tests/modules";' % topobjdir,
            '-e', 'const SERVER_PORT = "%s";' % port,
            '-e', 'const INCLUDE_FILES = [%s];' % ', '.join(head_paths),
            '-e', '_register_protocol_handlers();',
            '-e', 'for each (let name in INCLUDE_FILES) load(name);',
            '-e', '_fakeIdleService.activate();',
            '-f', js_file
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

    @Command('storage-server', category='services',
             description='Run a storage server.')
    @SyncStorageCommand
    def run_storage_server(self, port=DEFAULT_PORT, address=DEFAULT_HOSTNAME):
        exit(self.run_server(STORAGE_SERVER_SCRIPT, address, port))

    @Command('bagheera-server', category='services',
             description='Run a bagheera server.')
    def run_bagheera_server(self, port=DEFAULT_PORT, address=DEFAULT_HOSTNAME):
        exit(self.run_server(BAGHEERA_SERVER_SCRIPT, address, port))
