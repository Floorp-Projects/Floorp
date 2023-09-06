# coding=UTF-8

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile

import mozprofile

from marionette_driver import errors
from marionette_harness import MarionetteTestCase, parameterized


class BaseProfileManagement(MarionetteTestCase):
    def setUp(self):
        super(BaseProfileManagement, self).setUp()

        self.orig_profile_path = self.profile_path

    def tearDown(self):
        shutil.rmtree(self.orig_profile_path, ignore_errors=True)
        self.marionette.profile = None

        self.marionette.quit(in_app=False, clean=True)

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

        # Set a new workspace for the instance, which will be used
        # the next time a new profile is requested by a test.
        self.workspace = tempfile.mkdtemp()
        self.marionette.instance.workspace = self.workspace

    def tearDown(self):
        self.marionette.instance.workspace = None

        shutil.rmtree(self.workspace, ignore_errors=True)

        super(WorkspaceProfileManagement, self).tearDown()


class ExternalProfileMixin(object):
    def setUp(self):
        super(ExternalProfileMixin, self).setUp()

        # Create external profile
        tmp_dir = tempfile.mkdtemp(suffix="external")
        shutil.rmtree(tmp_dir, ignore_errors=True)

        # Re-use all the required profile arguments (preferences)
        profile_args = self.marionette.instance.profile_args
        profile_args["profile"] = tmp_dir
        self.external_profile = mozprofile.Profile(**profile_args)

        # Prevent profile from being removed during cleanup
        self.external_profile.create_new = False

    def tearDown(self):
        shutil.rmtree(self.external_profile.profile, ignore_errors=True)

        super(ExternalProfileMixin, self).tearDown()


class TestQuitRestartWithoutWorkspace(BaseProfileManagement):
    @parameterized("safe", True)
    @parameterized("forced", False)
    def test_quit_keeps_same_profile(self, in_app):
        self.marionette.quit(in_app=in_app)
        self.marionette.start_session()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_quit_clean_creates_new_profile(self):
        self.marionette.quit(in_app=False, clean=True)
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    @parameterized("safe", True)
    @parameterized("forced", False)
    def test_restart_keeps_same_profile(self, in_app):
        self.marionette.restart(in_app=in_app)

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_restart_clean_creates_new_profile(self):
        self.marionette.restart(in_app=False, clean=True)

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))


class TestQuitRestartWithWorkspace(WorkspaceProfileManagement):
    @parameterized("safe", True)
    @parameterized("forced", False)
    def test_quit_keeps_same_profile(self, in_app):
        self.marionette.quit(in_app=in_app)
        self.marionette.start_session()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertNotIn(self.workspace, self.profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_quit_clean_creates_new_profile(self):
        self.marionette.quit(in_app=False, clean=True)
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    @parameterized("safe", True)
    @parameterized("forced", False)
    def test_restart_keeps_same_profile(self, in_app):
        self.marionette.restart(in_app=in_app)

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertNotIn(self.workspace, self.profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_restart_clean_creates_new_profile(self):
        self.marionette.restart(in_app=False, clean=True)

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))


class TestSwitchProfileFailures(BaseProfileManagement):
    def test_raise_for_switching_profile_while_instance_is_running(self):
        with self.assertRaisesRegexp(
            errors.MarionetteException, "instance is not running"
        ):
            self.marionette.instance.switch_profile()


class TestSwitchProfileWithoutWorkspace(ExternalProfileMixin, BaseProfileManagement):
    def setUp(self):
        super(TestSwitchProfileWithoutWorkspace, self).setUp()

        self.marionette.quit()

    def test_do_not_call_cleanup_of_profile_for_path_only(self):
        # If a path to a profile has been given (eg. via the --profile command
        # line argument) and the profile hasn't been created yet, switching the
        # profile should not try to call `cleanup()` on a string.
        self.marionette.instance._profile = self.external_profile.profile
        self.marionette.instance.switch_profile()

    def test_new_random_profile_name(self):
        self.marionette.instance.switch_profile()
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_new_named_profile(self):
        self.marionette.instance.switch_profile("foobar")
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn("foobar", self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_new_named_profile_unicode(self):
        """Test using unicode string with 1-4 bytes encoding works."""
        self.marionette.instance.switch_profile("$¢€🍪")
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn("$¢€🍪", self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_new_named_profile_unicode_escape_characters(self):
        """Test using escaped unicode string with 1-4 bytes encoding works."""
        self.marionette.instance.switch_profile("\u0024\u00A2\u20AC\u1F36A")
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn("\u0024\u00A2\u20AC\u1F36A", self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_clone_existing_profile(self):
        self.marionette.instance.switch_profile(clone_from=self.external_profile)
        self.marionette.start_session()

        self.assertIn(
            os.path.basename(self.external_profile.profile), self.profile_path
        )
        self.assertTrue(os.path.exists(self.external_profile.profile))

    def test_replace_with_current_profile(self):
        self.marionette.instance.profile = self.profile
        self.marionette.start_session()

        self.assertEqual(self.profile_path, self.orig_profile_path)
        self.assertTrue(os.path.exists(self.orig_profile_path))

    def test_replace_with_external_profile(self):
        self.marionette.instance.profile = self.external_profile
        self.marionette.start_session()

        self.assertEqual(self.profile_path, self.external_profile.profile)
        self.assertFalse(os.path.exists(self.orig_profile_path))

        # Check that required preferences have been correctly set
        self.assertFalse(self.marionette.get_pref("remote.prefs.recommended"))

        # Set a new profile and ensure the external profile has not been deleted
        self.marionette.quit()
        self.marionette.instance.profile = None

        self.assertNotEqual(self.profile_path, self.external_profile.profile)
        self.assertTrue(os.path.exists(self.external_profile.profile))


class TestSwitchProfileWithWorkspace(ExternalProfileMixin, WorkspaceProfileManagement):
    def setUp(self):
        super(TestSwitchProfileWithWorkspace, self).setUp()

        self.marionette.quit()

    def test_new_random_profile_name(self):
        self.marionette.instance.switch_profile()
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_new_named_profile(self):
        self.marionette.instance.switch_profile("foobar")
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn("foobar", self.profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertFalse(os.path.exists(self.orig_profile_path))

    def test_clone_existing_profile(self):
        self.marionette.instance.switch_profile(clone_from=self.external_profile)
        self.marionette.start_session()

        self.assertNotEqual(self.profile_path, self.orig_profile_path)
        self.assertIn(self.workspace, self.profile_path)
        self.assertIn(
            os.path.basename(self.external_profile.profile), self.profile_path
        )
        self.assertTrue(os.path.exists(self.external_profile.profile))
