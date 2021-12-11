# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys
from unittest import skipIf

from marionette_driver.addons import Addons, AddonInstallException
from marionette_driver.errors import UnknownException
from marionette_harness import MarionetteTestCase


here = os.path.abspath(os.path.dirname(__file__))


class TestAddons(MarionetteTestCase):
    def setUp(self):
        super(TestAddons, self).setUp()

        self.addons = Addons(self.marionette)
        self.preinstalled_addons = self.all_addon_ids

    def tearDown(self):
        self.reset_addons()

        super(TestAddons, self).tearDown()

    @property
    def all_addon_ids(self):
        with self.marionette.using_context("chrome"):
            addons = self.marionette.execute_async_script(
                """
              let [resolve] = arguments;
              Components.utils.import("resource://gre/modules/AddonManager.jsm");

              AddonManager.getAllAddons().then(function(addons) {
                let ids = addons.map(x => x.id);
                resolve(ids);
              });
            """
            )

        return set(addons)

    def reset_addons(self):
        with self.marionette.using_context("chrome"):
            for addon in self.all_addon_ids - self.preinstalled_addons:
                addon_id = self.marionette.execute_async_script(
                    """
                  let [resolve] = arguments;
                  Components.utils.import("resource://gre/modules/AddonManager.jsm");

                  return new Promise(await resolve => {
                    let addon = await AddonManager.getAddonByID(arguments[0]);
                    addon.uninstall();
                    resolve(addon.id);
                  });
                """,
                    script_args=(addon,),
                )
                self.assertEqual(
                    addon_id, addon, msg="Failed to uninstall {}".format(addon)
                )

    def test_temporary_install_and_remove_unsigned_addon(self):
        addon_path = os.path.join(here, "webextension-unsigned.xpi")

        addon_id = self.addons.install(addon_path, temp=True)
        self.assertIn(addon_id, self.all_addon_ids)
        self.assertEqual(addon_id, "{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}")

        self.addons.uninstall(addon_id)
        self.assertNotIn(addon_id, self.all_addon_ids)

    def test_temporary_install_invalid_addon(self):
        addon_path = os.path.join(here, "webextension-invalid.xpi")

        with self.assertRaises(AddonInstallException):
            self.addons.install(addon_path, temp=True)
        self.assertNotIn("{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}", self.all_addon_ids)

    def test_install_and_remove_signed_addon(self):
        addon_path = os.path.join(here, "webextension-signed.xpi")

        addon_id = self.addons.install(addon_path)
        self.assertIn(addon_id, self.all_addon_ids)
        self.assertEqual(addon_id, "{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}")

        self.addons.uninstall(addon_id)
        self.assertNotIn(addon_id, self.all_addon_ids)

    def test_install_invalid_addon(self):
        addon_path = os.path.join(here, "webextension-invalid.xpi")

        with self.assertRaises(AddonInstallException):
            self.addons.install(addon_path)
        self.assertNotIn("{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}", self.all_addon_ids)

    def test_install_unsigned_addon_fails(self):
        addon_path = os.path.join(here, "webextension-unsigned.xpi")

        with self.assertRaises(AddonInstallException):
            self.addons.install(addon_path)

    def test_install_nonexistent_addon(self):
        addon_path = os.path.join(here, "does-not-exist.xpi")

        with self.assertRaises(AddonInstallException):
            self.addons.install(addon_path)

    def test_install_with_relative_path(self):
        with self.assertRaises(AddonInstallException):
            self.addons.install("webextension.xpi")

    @skipIf(sys.platform != "win32", "Only makes sense on Windows")
    def test_install_mixed_separator_windows(self):
        # Ensure the base path has only \
        addon_path = here.replace("/", "\\")
        addon_path += "/webextension-signed.xpi"

        addon_id = self.addons.install(addon_path, temp=True)
        self.assertIn(addon_id, self.all_addon_ids)
        self.assertEqual(addon_id, "{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}")

        self.addons.uninstall(addon_id)
        self.assertNotIn(addon_id, self.all_addon_ids)

    def test_uninstall_nonexistent_addon(self):
        with self.assertRaises(UnknownException):
            self.addons.uninstall("i-do-not-exist-as-an-id")
