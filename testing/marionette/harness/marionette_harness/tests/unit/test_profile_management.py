# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile

import mozprofile

from marionette_harness import MarionetteTestCase


class BaseProfileManagement(MarionetteTestCase):

    def setUp(self):
        super(BaseProfileManagement, self).setUp()

        self.orig_profile_path = self.profile_path

        # Create external profile and mark it as not-removable
        tmp_dir = tempfile.mkdtemp(suffix="external")
        shutil.rmtree(tmp_dir, ignore_errors=True)

        self.external_profile = mozprofile.Profile(profile=tmp_dir)
        self.external_profile.create_new = False

    def tearDown(self):
        shutil.rmtree(self.external_profile.profile, ignore_errors=True)

        self.marionette.profile = None

        super(BaseProfileManagement, self).tearDown()

    @property
    def profile(self):
        return self.marionette.instance.profile

    @property
    def profile_path(self):
        return self.marionette.instance.profile.profile


class WorkspaceProfileManagement(BaseProfileManagement):

    def setUp(self):
        super(WorkspaceProfileManagement, self).setUp()

        self.workspace = tempfile.mkdtemp()
        self.marionette.instance.workspace = self.workspace

    def tearDown(self):
        self.marionette.instance.workspace = None

        shutil.rmtree(self.workspace, ignore_errors=True)

        super(WorkspaceProfileManagement, self).tearDown()


class TestQuitRestartWithoutWorkspace(BaseProfileManagement):

    def test_quit_keeps_same_profile(self):
        self.marionette.quit()
        self.marionette.start_session()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_quit_clean_creates_new_profile(self):
        self.marionette.quit(clean=True)
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_restart_keeps_same_profile(self):
        self.marionette.restart()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_restart_clean_creates_new_profile(self):
        self.marionette.restart(clean=True)

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))


class TestQuitRestartWithWorkspace(WorkspaceProfileManagement):

    def test_quit_keeps_same_profile(self):
        self.marionette.quit()
        self.marionette.start_session()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertNotIn(self.workspace, self.profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_quit_clean_creates_new_profile(self):
        self.marionette.quit(clean=True)
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_restart_keeps_same_profile(self):
        self.marionette.restart()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertNotIn(self.workspace, self.profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_restart_clean_creates_new_profile(self):
        self.marionette.restart(clean=True)

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))


class TestSwitchProfileWithoutWorkspace(BaseProfileManagement):

    def setUp(self):
        super(TestSwitchProfileWithoutWorkspace, self).setUp()

        self.marionette.quit()

    def tearDown(self):
        super(TestSwitchProfileWithoutWorkspace, self).tearDown()

    def test_new_random_profile_name(self):
        self.marionette.instance.switch_profile()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_new_named_profile(self):
        self.marionette.instance.switch_profile("foobar")

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn("foobar", self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_clone_existing_profile(self):
        self.marionette.instance.switch_profile(clone_from=self.external_profile)

        self.assertIn(os.path.basename(self.external_profile.profile), self.profile_path)
        self.assertTrue(os.path.exists(self.external_profile.profile))

    def test_replace_with_current_profile(self):
        self.marionette.instance.profile = self.profile

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_replace_with_external_profile(self):
        self.marionette.instance.profile = self.external_profile

        self.assertEqual(self.profile_path, self.external_profile.profile)
        self.assertFalse(os.path.exists(self.orig_profile_path))

        # Set a new profile and ensure the external profile has not been deleted
        self.marionette.instance.profile = None

        self.assertNotEqual(self.profile_path, self.external_profile.profile)
        self.assertTrue(os.path.exists(self.external_profile.profile))


class TestSwitchProfileWithWorkspace(WorkspaceProfileManagement):

    def setUp(self):
        super(TestSwitchProfileWithWorkspace, self).setUp()

        self.marionette.quit()

    def test_new_random_profile_name(self):
        self.marionette.instance.switch_profile()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_new_named_profile(self):
        self.marionette.instance.switch_profile("foobar")

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn("foobar", self.profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_clone_existing_profile(self):
        self.marionette.instance.switch_profile(clone_from=self.external_profile)

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertIn(os.path.basename(self.external_profile.profile), self.profile_path)
        self.assertTrue(os.path.exists(self.external_profile.profile))
