# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path

from marionette_harness import MarionetteTestCase


class MovedOriginDirectoryCleanupTestCase(MarionetteTestCase):
    def setUp(self):
        super().setUp()
        self.marionette.enforce_gecko_prefs(
            {
                "privacy.sanitize.sanitizeOnShutdown": True,
                "privacy.clearOnShutdown.offlineApps": True,
            }
        )
        self.moved_origin_directory = (
            Path(self.marionette.profile_path) / "storage" / "to-be-removed" / "foo"
        )
        self.moved_origin_directory.mkdir(parents=True)

    def test_ensure_cleanup_by_quit(self):
        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must exist",
        )

        # Cleanup happens via Sanitizer.sanitizeOnShutdown
        self.marionette.quit()

        self.assertFalse(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must disappear",
        )

    def test_ensure_cleanup_at_crashed_restart(self):
        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must exist",
        )

        # Cleanup happens via Sanitizer.onStartup
        self.marionette.restart(in_app=False)

        self.assertFalse(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must disappear",
        )
