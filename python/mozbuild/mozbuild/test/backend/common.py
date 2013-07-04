# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from shutil import rmtree
from tempfile import mkdtemp

from mach.logging import LoggingManager

from mozbuild.backend.configenvironment import ConfigEnvironment
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader


log_manager = LoggingManager()
log_manager.add_terminal_logging()


test_data_path = os.path.abspath(os.path.dirname(__file__))
test_data_path = os.path.join(test_data_path, 'data')


CONFIGS = {
    'stub0': {
        'defines': [
            ('MOZ_TRUE_1', '1'),
            ('MOZ_TRUE_2', '1'),
        ],
        'non_global_defines': [
            ('MOZ_NONGLOBAL_1', '1'),
            ('MOZ_NONGLOBAL_2', '1'),
        ],
        'substs': [
            ('MOZ_FOO', 'foo'),
            ('MOZ_BAR', 'bar'),
        ],
    },
    'external_make_dirs': {
        'defines': [],
        'non_global_defines': [],
        'substs': [],
    },
    'substitute_config_files': {
        'defines': [],
        'non_global_defines': [],
        'substs': [
            ('MOZ_FOO', 'foo'),
            ('MOZ_BAR', 'bar'),
        ],
    },
    'variable_passthru': {
        'defines': [],
        'non_global_defines': [],
        'substs': [],
    },
    'exports': {
        'defines': [],
        'non_global_defines': [],
        'substs': [],
    },
    'xpcshell_manifests': {
        'defines': [],
        'non_global_defines': [],
        'substs': [
            ('XPCSHELL_TESTS_MANIFESTS', 'XPCSHELL_TESTS'),
            ],
    },
}


class BackendTester(unittest.TestCase):
    def _get_environment(self, name):
        """Obtain a new instance of a ConfigEnvironment for a known profile.

        A new temporary object directory is created for the environment. The
        environment is cleaned up automatically when the test finishes.
        """
        config = CONFIGS[name]

        objdir = mkdtemp()
        self.addCleanup(rmtree, objdir)

        srcdir = os.path.join(test_data_path, name)
        config['substs'].append(('top_srcdir', srcdir))
        return ConfigEnvironment(srcdir, objdir, **config)

    def _emit(self, name, env=None):
        if not env:
            env = self._get_environment(name)

        reader = BuildReader(env)
        emitter = TreeMetadataEmitter(env)

        return env, emitter.emit(reader.read_topsrcdir())

    def _consume(self, name, cls, env=None):
        env, objs = self._emit(name, env=env)
        backend = cls(env)
        backend.consume(objs)

        return env

    def _tree_paths(self, topdir, filename):
        for dirpath, dirnames, filenames in os.walk(topdir):
            for f in filenames:
                if f == filename:
                    yield os.path.relpath(os.path.join(dirpath, f), topdir)

    def _mozbuild_paths(self, env):
        return self._tree_paths(env.topsrcdir, 'moz.build')

    def _makefile_in_paths(self, env):
        return self._tree_paths(env.topsrcdir, 'Makefile.in')


__all__ = ['BackendTester']
