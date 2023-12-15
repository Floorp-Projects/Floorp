# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozpack.path as mozpath
import six.moves.cPickle as pickle
from mozunit import main

from mozbuild.backend.test_manifest import TestManifestBackend
from mozbuild.test.backend.common import BackendTester


class TestTestManifestBackend(BackendTester):
    def test_all_tests_metadata_file_written(self):
        """Ensure all-tests.pkl is generated."""
        env = self._consume("test-manifests-written", TestManifestBackend)

        all_tests_path = mozpath.join(env.topobjdir, "all-tests.pkl")
        self.assertTrue(os.path.exists(all_tests_path))

        with open(all_tests_path, "rb") as fh:
            o = pickle.load(fh)

        self.assertIn("xpcshell.js", o)
        self.assertIn("dir1/test_bar.js", o)

        self.assertEqual(len(o["xpcshell.js"]), 1)

    def test_test_installs_metadata_file_written(self):
        """Ensure test-installs.pkl is generated."""
        env = self._consume("test-manifest-shared-support", TestManifestBackend)
        all_tests_path = mozpath.join(env.topobjdir, "all-tests.pkl")
        self.assertTrue(os.path.exists(all_tests_path))
        test_installs_path = mozpath.join(env.topobjdir, "test-installs.pkl")

        with open(test_installs_path, "rb") as fh:
            test_installs = pickle.load(fh)

        self.assertEqual(
            set(test_installs.keys()),
            set(["child/test_sub.js", "child/data/**", "child/another-file.sjs"]),
        )

        for key in test_installs.keys():
            self.assertIn(key, test_installs)

    def test_test_defaults_metadata_file_written(self):
        """Ensure test-defaults.pkl is generated."""
        env = self._consume("test-manifests-written", TestManifestBackend)

        test_defaults_path = mozpath.join(env.topobjdir, "test-defaults.pkl")
        self.assertTrue(os.path.exists(test_defaults_path))

        with open(test_defaults_path, "rb") as fh:
            o = {mozpath.normpath(k): v for k, v in pickle.load(fh).items()}

        self.assertEqual(
            set(mozpath.relpath(k, env.topsrcdir) for k in o.keys()),
            set(["dir1/xpcshell.toml", "xpcshell.toml", "mochitest.toml"]),
        )

        manifest_path = mozpath.join(env.topsrcdir, "xpcshell.toml")
        self.assertIn("here", o[manifest_path])
        self.assertIn("support-files", o[manifest_path])

    def test_test_manifest_sources(self):
        """Ensure that backend sources are generated correctly."""
        env = self._consume("test-manifests-backend-sources", TestManifestBackend)

        backend_path = mozpath.join(env.topobjdir, "backend.TestManifestBackend.in")
        self.assertTrue(os.path.exists(backend_path))

        status_path = mozpath.join(env.topobjdir, "config.status")

        with open(backend_path, "r") as fh:
            sources = set(source.strip() for source in fh)

        self.assertEqual(
            sources,
            set(
                [
                    mozpath.join(env.topsrcdir, "mochitest.toml"),
                    mozpath.join(env.topsrcdir, "mochitest-common.toml"),
                    mozpath.join(env.topsrcdir, "moz.build"),
                    status_path,
                ]
            ),
        )


if __name__ == "__main__":
    main()
