# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class MovedOriginDirectoryCleanupTestCase(MarionetteTestCase):
    def setUp(self):
        super().setUp()
        self.marionette.enforce_gecko_prefs(
            {
                "privacy.sanitize.sanitizeOnShutdown": True,
                "privacy.clearOnShutdown.offlineApps": True,
                "dom.quotaManager.backgroundTask.enabled": False,
            }
        )
        self.moved_origin_directory = (
            Path(self.marionette.profile_path) / "storage" / "to-be-removed" / "foo"
        )
        self.moved_origin_directory.mkdir(parents=True, exist_ok=True)
        self.to_be_removed_directory = (
            Path(self.marionette.profile_path) / "storage" / "to-be-removed"
        )

        # Add a cookie to get a principal to be cleaned up
        with self.marionette.using_context("chrome"):
            self.marionette.execute_script(
                """
                Services.cookies.add(
                    "example.local",
                    "path",
                    "name",
                    "value",
                    false,
                    false,
                    false,
                    Math.floor(Date.now() / 1000) + 24 * 60 * 60,
                    {},
                    Ci.nsICookie.SAMESITE_NONE,
                    Ci.nsICookie.SCHEME_UNSET
                );
                """
            )

    def removeAllCookies(self):
        with self.marionette.using_context("chrome"):
            self.marionette.execute_script(
                """
                Services.cookies.removeAll();
                """
            )

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

        # Pending sanitization is added via Sanitizer.onStartup
        # "offlineApps" corresponds to CLEAR_DOM_STORAGES
        Wait(self.marionette).until(
            lambda _: (
                "offlineApps" in self.marionette.get_pref("privacy.sanitize.pending"),
            ),
            message="privacy.sanitize.pending must include offlineApps",
        )

        # Cleanup happens via Sanitizer.onStartup after restart
        self.marionette.restart(in_app=False)

        # Wait longer for 30 sec for the restart to finish, given bug 1814281.
        Wait(self.marionette, timeout=30).until(
            lambda _: not self.moved_origin_directory.exists(),
            message="to-be-removed subdirectory must disappear",
        )

    def test_ensure_cleanup_by_quit_with_background_task(self):
        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must exist",
        )

        self.marionette.set_pref("dom.quotaManager.backgroundTask.enabled", True)

        # Cleanup happens via Sanitizer.sanitizeOnShutdown
        self.marionette.quit()

        Wait(self.marionette).until(
            lambda _: not self.moved_origin_directory.exists(),
            message="to-be-removed subdirectory must disappear",
        )
        self.assertTrue(
            self.to_be_removed_directory.exists(),
            "to-be-removed parent directory should still be alive",
        )

    def test_ensure_no_cleanup_when_disabled(self):
        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must exist",
        )

        self.marionette.set_pref("privacy.sanitize.sanitizeOnShutdown", False)
        self.marionette.quit()

        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must not disappear",
        )

    def test_ensure_no_cleanup_when_no_cookie(self):
        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must exist",
        )

        self.removeAllCookies()

        self.marionette.quit()

        self.assertTrue(
            self.moved_origin_directory.exists(),
            "to-be-removed subdirectory must not disappear",
        )
