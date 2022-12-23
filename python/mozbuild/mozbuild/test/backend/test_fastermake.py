# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozpack.path as mozpath
from mozpack.copier import FileRegistry
from mozpack.manifests import InstallManifest
from mozunit import main

from mozbuild.backend.fastermake import FasterMakeBackend
from mozbuild.test.backend.common import BackendTester


class TestFasterMakeBackend(BackendTester):
    def test_basic(self):
        """Ensure the FasterMakeBackend works without error."""
        env = self._consume("stub0", FasterMakeBackend)
        self.assertTrue(
            os.path.exists(mozpath.join(env.topobjdir, "backend.FasterMakeBackend"))
        )
        self.assertTrue(
            os.path.exists(mozpath.join(env.topobjdir, "backend.FasterMakeBackend.in"))
        )

    def test_final_target_files_wildcard(self):
        """Ensure that wildcards in FINAL_TARGET_FILES work properly."""
        env = self._consume("final-target-files-wildcard", FasterMakeBackend)
        m = InstallManifest(
            path=mozpath.join(env.topobjdir, "faster", "install_dist_bin")
        )
        self.assertEqual(len(m), 1)
        reg = FileRegistry()
        m.populate_registry(reg)
        expected = [("foo/bar.xyz", "bar.xyz"), ("foo/foo.xyz", "foo.xyz")]
        actual = [(path, mozpath.relpath(f.path, env.topsrcdir)) for (path, f) in reg]
        self.assertEqual(expected, actual)


if __name__ == "__main__":
    main()
