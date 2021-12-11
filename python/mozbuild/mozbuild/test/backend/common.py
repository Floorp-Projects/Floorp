# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import unittest

from collections import defaultdict
from shutil import rmtree
from tempfile import mkdtemp

from mach.logging import LoggingManager

from mozbuild.backend.configenvironment import ConfigEnvironment
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader

import mozpack.path as mozpath


log_manager = LoggingManager()
log_manager.add_terminal_logging()


test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, "data")


CONFIGS = defaultdict(
    lambda: {
        "defines": {},
        "substs": {"OS_TARGET": "WINNT"},
    },
    {
        "binary-components": {
            "defines": {},
            "substs": {
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
                "COMPILE_ENVIRONMENT": "1",
            },
        },
        "database": {
            "defines": {},
            "substs": {
                "CC": "clang",
                "CXX": "clang++",
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
            },
        },
        "rust-library": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "RUST_TARGET": "x86_64-unknown-linux-gnu",
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
            },
        },
        "host-rust-library": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "RUST_HOST_TARGET": "x86_64-unknown-linux-gnu",
                "RUST_TARGET": "armv7-linux-androideabi",
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
            },
        },
        "host-rust-library-features": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "RUST_HOST_TARGET": "x86_64-unknown-linux-gnu",
                "RUST_TARGET": "armv7-linux-androideabi",
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
            },
        },
        "rust-library-features": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "RUST_TARGET": "x86_64-unknown-linux-gnu",
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
            },
        },
        "rust-programs": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "RUST_TARGET": "i686-pc-windows-msvc",
                "RUST_HOST_TARGET": "i686-pc-windows-msvc",
                "BIN_SUFFIX": ".exe",
                "HOST_BIN_SUFFIX": ".exe",
            },
        },
        "test-support-binaries-tracked": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "LIB_SUFFIX": "dll",
                "BIN_SUFFIX": ".exe",
            },
        },
        "sources": {
            "defines": {},
            "substs": {
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
            },
        },
        "stub0": {
            "defines": {
                "MOZ_TRUE_1": "1",
                "MOZ_TRUE_2": "1",
            },
            "substs": {
                "MOZ_FOO": "foo",
                "MOZ_BAR": "bar",
            },
        },
        "substitute_config_files": {
            "defines": {},
            "substs": {
                "MOZ_FOO": "foo",
                "MOZ_BAR": "bar",
            },
        },
        "test_config": {
            "defines": {
                "foo": "baz qux",
                "baz": 1,
            },
            "substs": {
                "foo": "bar baz",
            },
        },
        "visual-studio": {
            "defines": {},
            "substs": {
                "MOZ_APP_NAME": "my_app",
            },
        },
        "prog-lib-c-only": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "LIB_SUFFIX": ".a",
                "BIN_SUFFIX": "",
            },
        },
        "gn-processor": {
            "defines": {},
            "substs": {
                "BUILD_BACKENDS": [
                    "GnMozbuildWriter",
                    "RecursiveMake",
                ],
                "COMPILE_ENVIRONMENT": "1",
                "STL_FLAGS": [],
                "RUST_TARGET": "x86_64-unknown-linux-gnu",
                "LIB_PREFIX": "lib",
                "LIB_SUFFIX": "a",
                "OS_TARGET": "Darwin",
            },
        },
        "ipdl_sources": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "LIB_SUFFIX": ".a",
                "BIN_SUFFIX": "",
            },
        },
        "program-paths": {
            "defines": {},
            "substs": {
                "COMPILE_ENVIRONMENT": "1",
                "BIN_SUFFIX": ".prog",
            },
        },
        "linkage": {
            "defines": {},
            "substs": {
                "CC_TYPE": "clang",
                "COMPILE_ENVIRONMENT": "1",
                "LIB_SUFFIX": "a",
                "BIN_SUFFIX": ".exe",
                "DLL_SUFFIX": ".so",
                "OBJ_SUFFIX": "o",
                "EXPAND_LIBS_LIST_STYLE": "list",
            },
        },
    },
)


class BackendTester(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop("MOZ_OBJDIR", None)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

    def _get_environment(self, name):
        """Obtain a new instance of a ConfigEnvironment for a known profile.

        A new temporary object directory is created for the environment. The
        environment is cleaned up automatically when the test finishes.
        """
        config = CONFIGS[name]
        config["substs"]["MOZ_UI_LOCALE"] = "en-US"

        srcdir = mozpath.join(test_data_path, name)
        config["substs"]["top_srcdir"] = srcdir

        # Create the objdir in the srcdir to ensure that they share the
        # same drive on Windows.
        objdir = mkdtemp(dir=srcdir)
        self.addCleanup(rmtree, objdir)

        return ConfigEnvironment(srcdir, objdir, **config)

    def _emit(self, name, env=None):
        env = env or self._get_environment(name)
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
                    yield mozpath.relpath(mozpath.join(dirpath, f), topdir)

    def _mozbuild_paths(self, env):
        return self._tree_paths(env.topsrcdir, "moz.build")

    def _makefile_in_paths(self, env):
        return self._tree_paths(env.topsrcdir, "Makefile.in")


__all__ = ["BackendTester"]
