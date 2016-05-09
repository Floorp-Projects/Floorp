# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pprint
from datetime import datetime

from marionette import MarionetteTestCase
from marionette_driver import Wait

from firefox_puppeteer.api.prefs import Preferences
from firefox_puppeteer.api.software_update import SoftwareUpdate
from firefox_puppeteer.testcases import BaseFirefoxTestCase
from firefox_puppeteer.ui.update_wizard import UpdateWizardDialog


class FirefoxTestCase(BaseFirefoxTestCase, MarionetteTestCase):
    """ Integrate MarionetteTestCase with BaseFirefoxTestCase by reordering MRO """
    pass


class UpdateTestCase(FirefoxTestCase):

    TIMEOUT_UPDATE_APPLY = 300
    TIMEOUT_UPDATE_CHECK = 30
    TIMEOUT_UPDATE_DOWNLOAD = 360

    # For the old update wizard, the errors are displayed inside the dialog. For the
    # handling of updates in the about window the errors are displayed in new dialogs.
    # When the old wizard is open we have to set the preference, so the errors will be
    # shown as expected, otherwise we would have unhandled modal dialogs when errors are
    # raised. See:
    # http://mxr.mozilla.org/mozilla-central/source/toolkit/mozapps/update/nsUpdateService.js?rev=a9240b1eb2fb#4813
    # http://mxr.mozilla.org/mozilla-central/source/toolkit/mozapps/update/nsUpdateService.js?rev=a9240b1eb2fb#4756
    PREF_APP_UPDATE_ALTWINDOWTYPE = 'app.update.altwindowtype'

    def __init__(self, *args, **kwargs):
        super(UpdateTestCase, self).__init__(*args, **kwargs)

        self.target_buildid = kwargs.pop('update_target_buildid')
        self.target_version = kwargs.pop('update_target_version')

        self.update_channel = kwargs.pop('update_channel')
        self.default_update_channel = None

        self.update_mar_channels = set(kwargs.pop('update_mar_channels'))
        self.default_mar_channels = None

        self.updates = []

    def setUp(self, is_fallback=False):
        super(UpdateTestCase, self).setUp()

        self.software_update = SoftwareUpdate(lambda: self.marionette)
        self.download_duration = None

        # Bug 604364 - Preparation to test multiple update steps
        self.current_update_index = 0

        self.staging_directory = self.software_update.staging_directory

        # If requested modify the default update channel. It will be active
        # after the next restart of the application
        # Bug 1142805 - Modify file via Python directly
        if self.update_channel:
            # Backup the original content and the path of the channel-prefs.js file
            self.default_update_channel = {
                'content': self.software_update.update_channel.file_contents,
                'path': self.software_update.update_channel.file_path,
            }
            self.software_update.update_channel.default_channel = self.update_channel

        # If requested modify the list of allowed MAR channels
        # Bug 1142805 - Modify file via Python directly
        if self.update_mar_channels:
            # Backup the original content and the path of the update-settings.ini file
            self.default_mar_channels = {
                'content': self.software_update.mar_channels.config_file_contents,
                'path': self.software_update.mar_channels.config_file_path,
            }
            self.software_update.mar_channels.add_channels(self.update_mar_channels)

        # Bug 1142805 - Until we don't modify the channel-prefs.js and update-settings.ini
        # files before Firefox gets started, a restart of Firefox is necessary to
        # accept the new update channel.
        self.restart()

        # Dictionary which holds the information for each update
        self.updates = [{
            'build_pre': self.software_update.build_info,
            'build_post': None,
            'fallback': is_fallback,
            'patch': {},
            'success': False,
        }]

        self.assertEqual(self.software_update.update_channel.default_channel,
                         self.software_update.update_channel.channel)

        self.assertTrue(self.update_mar_channels.issubset(
                        self.software_update.mar_channels.channels),
                        'Allowed MAR channels have been set: expected "{}" in "{}"'.format(
                            ', '.join(self.update_mar_channels),
                            ', '.join(self.software_update.mar_channels.channels)))

        # Check if the user has permissions to run the update
        self.assertTrue(self.software_update.allowed,
                        'Current user has permissions to update the application.')

    def tearDown(self):
        try:
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.selected_tab])

            # Print results for now until we have treeherder integration
            output = pprint.pformat(self.updates)
            self.logger.info('Update test results: \n{}'.format(output))

        finally:
            super(UpdateTestCase, self).tearDown()

            self.restore_config_files()

    @property
    def patch_info(self):
        """ Returns information about the active update in the queue.

        :returns: A dictionary with information about the active patch
        """
        patch = self.software_update.patch_info
        patch['download_duration'] = self.download_duration

        return patch

    def check_for_updates(self, about_window, timeout=TIMEOUT_UPDATE_CHECK):
        """Clicks on "Check for Updates" button, and waits for check to complete.

        :param about_window: Instance of :class:`AboutWindow`.
        :param timeout: How long to wait for the update check to finish. Optional,
         defaults to 60s.

        :returns: True, if an update is available.
        """
        self.assertEqual(about_window.deck.selected_panel,
                         about_window.deck.check_for_updates)

        about_window.deck.check_for_updates.button.click()
        Wait(self.marionette, timeout=self.TIMEOUT_UPDATE_CHECK).until(
            lambda _: about_window.deck.selected_panel not in
            (about_window.deck.check_for_updates, about_window.deck.checking_for_updates),
            message='Check for updates has been finished.')

        return about_window.deck.selected_panel != about_window.deck.no_updates_found

    def check_update_applied(self):
        self.updates[self.current_update_index]['build_post'] = self.software_update.build_info

        about_window = self.browser.open_about_window()
        try:
            update_available = self.check_for_updates(about_window)

            # No further updates should be offered now with the same update type
            if update_available:
                self.download_update(about_window, wait_for_finish=False)
                self.assertNotEqual(self.software_update.active_update.type,
                                    self.updates[self.current_update_index].type)

            # Check that the update has been applied correctly
            update = self.updates[self.current_update_index]

            # The upgraded version should be identical with the version given by
            # the update and we shouldn't have run a downgrade
            check = self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");

              return  Services.vc.compare(arguments[0], arguments[1]);
            """, script_args=[update['build_post']['version'], update['build_pre']['version']])

            self.assertGreaterEqual(check, 0,
                                    'The version of the upgraded build is higher or equal')

            # If a target version has been specified, check if it matches the updated build
            if self.target_version:
                self.assertEqual(update['build_post']['version'], self.target_version)

            # The post buildid should be identical with the buildid contained in the patch
            self.assertEqual(update['build_post']['buildid'], update['patch']['buildid'])

            # If a target buildid has been specified, check if it matches the updated build
            if self.target_buildid:
                self.assertEqual(update['build_post']['buildid'], self.target_buildid)

            # An upgrade should not change the builds locale
            self.assertEqual(update['build_post']['locale'], update['build_pre']['locale'])

            # Check that no application-wide add-ons have been disabled
            self.assertEqual(update['build_post']['disabled_addons'],
                             update['build_pre']['disabled_addons'])

            update['success'] = True

        finally:
            about_window.close()

    def download_update(self, window, wait_for_finish=True, timeout=TIMEOUT_UPDATE_DOWNLOAD):
        """ Download the update patch.

        :param window: Instance of :class:`AboutWindow` or :class:`UpdateWizardDialog`.
        :param wait_for_finish: If True the function has to wait for the download to be finished.
         Optional, default to `True`.
        :param timeout: How long to wait for the download to finish. Optional, default to 360s.
        """

        def download_via_update_wizard(dialog):
            """ Download the update via the old update wizard dialog.

            :param dialog: Instance of :class:`UpdateWizardDialog`.
            """
            prefs = Preferences(lambda: self.marionette)
            prefs.set_pref(self.PREF_APP_UPDATE_ALTWINDOWTYPE, dialog.window_type)

            try:
                # If updates have already been found, proceed to download
                if dialog.wizard.selected_panel in [dialog.wizard.updates_found_basic,
                                                    dialog.wizard.updates_found_billboard,
                                                    dialog.wizard.error_patching,
                                                    ]:
                    dialog.select_next_page()

                # If incompatible add-on are installed, skip over the wizard page
                # TODO: Remove once we no longer support version Firefox 45.0ESR
                if self.utils.compare_version(self.appinfo.version, '49.0a1') == -1:
                    if dialog.wizard.selected_panel == dialog.wizard.incompatible_list:
                        dialog.select_next_page()

                # Updates were stored in the cache, so no download is necessary
                if dialog.wizard.selected_panel in [dialog.wizard.finished,
                                                    dialog.wizard.finished_background,
                                                    ]:
                    pass

                # Download the update
                elif dialog.wizard.selected_panel == dialog.wizard.downloading:
                    if wait_for_finish:
                        start_time = datetime.now()
                        self.wait_for_download_finished(dialog, timeout)
                        self.download_duration = (datetime.now() - start_time).total_seconds()

                        Wait(self.marionette).until(lambda _: (
                            dialog.wizard.selected_panel in [dialog.wizard.finished,
                                                             dialog.wizard.finished_background,
                                                             ]),
                                                    message='Final wizard page has been selected.')

                else:
                    raise Exception('Invalid wizard page for downloading an update: {}'.format(
                                    dialog.wizard.selected_panel))

            finally:
                prefs.restore_pref(self.PREF_APP_UPDATE_ALTWINDOWTYPE)

        # The old update wizard dialog has to be handled differently. It's necessary
        # for fallback updates and invalid add-on versions.
        if isinstance(window, UpdateWizardDialog):
            download_via_update_wizard(window)
            return

        if window.deck.selected_panel == window.deck.download_and_install:
            window.deck.download_and_install.button.click()

            # Wait for the download to start
            Wait(self.marionette).until(lambda _: (
                window.deck.selected_panel != window.deck.download_and_install),
                message='Download of the update has been started.')

        # In case of update failures, clicking the update button will open the
        # old update wizard dialog.
        if window.deck.selected_panel == window.deck.apply_billboard:
            dialog = self.browser.open_window(
                callback=lambda _: window.deck.update_button.click(),
                expected_window_class=UpdateWizardDialog
            )
            Wait(self.marionette).until(
                lambda _: dialog.wizard.selected_panel == dialog.wizard.updates_found_basic,
                message='An update has been found.')

            download_via_update_wizard(dialog)
            dialog.close()

            return

        if wait_for_finish:
            start_time = datetime.now()
            self.wait_for_download_finished(window, timeout)
            self.download_duration = (datetime.now() - start_time).total_seconds()

    def download_and_apply_available_update(self, force_fallback=False):
        """Checks, downloads, and applies an available update.

        :param force_fallback: Optional, if `True` invalidate current update status.
         Defaults to `False`.
        """
        # Open the about window and check for updates
        about_window = self.browser.open_about_window()

        try:
            update_available = self.check_for_updates(about_window)
            self.assertTrue(update_available,
                            "Available update has been found")

            # Download update and wait until it has been applied
            self.download_update(about_window)
            self.wait_for_update_applied(about_window)

        finally:
            self.updates[self.current_update_index]['patch'] = self.patch_info

        if force_fallback:
            # Set the downloaded update into failed state
            self.software_update.force_fallback()

        # Restart Firefox to apply the downloaded update
        self.restart()

    def download_and_apply_forced_update(self):
        # The update wizard dialog opens automatically after the restart
        dialog = self.windows.switch_to(lambda win: type(win) is UpdateWizardDialog)

        # In case of a broken complete update the about window has to be used
        if self.updates[self.current_update_index]['patch']['is_complete']:
            about_window = None
            try:
                self.assertEqual(dialog.wizard.selected_panel,
                                 dialog.wizard.error)
                dialog.close()

                # Open the about window and check for updates
                about_window = self.browser.open_about_window()
                update_available = self.check_for_updates(about_window)
                self.assertTrue(update_available,
                                'Available update has been found')

                # Download update and wait until it has been applied
                self.download_update(about_window)
                self.wait_for_update_applied(about_window)

            finally:
                if about_window:
                    self.updates[self.current_update_index]['patch'] = self.patch_info

        else:
            try:
                self.assertEqual(dialog.wizard.selected_panel,
                                 dialog.wizard.error_patching)

                # Start downloading the fallback update
                self.download_update(dialog)
                dialog.close()

            finally:
                self.updates[self.current_update_index]['patch'] = self.patch_info

        # Restart Firefox to apply the update
        self.restart()

    def restore_config_files(self):
        # Reset channel-prefs.js file if modified
        try:
            if self.default_update_channel:
                path = self.default_update_channel['path']
                self.logger.info('Restoring channel defaults for: {}'.format(path))
                with open(path, 'w') as f:
                    f.write(self.default_update_channel['content'])
        except IOError:
            self.logger.error('Failed to reset the default update channel.',
                              exc_info=True)

        # Reset update-settings.ini file if modified
        try:
            if self.default_mar_channels:
                path = self.default_mar_channels['path']
                self.logger.info('Restoring mar channel defaults for: {}'.format(path))
                with open(path, 'w') as f:
                    f.write(self.default_mar_channels['content'])
        except IOError:
            self.logger.error('Failed to reset the default mar channels.',
                              exc_info=True)

    def wait_for_download_finished(self, window, timeout=TIMEOUT_UPDATE_DOWNLOAD):
        """ Waits until download is completed.

        :param window: Instance of :class:`AboutWindow` or :class:`UpdateWizardDialog`.
        :param timeout: How long to wait for the download to finish. Optional,
         default to 360 seconds.
        """
        # The old update wizard dialog has to be handled differently. It's necessary
        # for fallback updates and invalid add-on versions.
        if isinstance(window, UpdateWizardDialog):
            Wait(self.marionette, timeout=timeout).until(
                lambda _: window.wizard.selected_panel != window.wizard.downloading,
                message='Download has been completed.')

            self.assertNotIn(window.wizard.selected_panel,
                             [window.wizard.error, window.wizard.error_extra])
            return

        Wait(self.marionette, timeout=timeout).until(
            lambda _: window.deck.selected_panel not in
            (window.deck.download_and_install, window.deck.downloading),
            message='Download has been completed.')

        self.assertNotEqual(window.deck.selected_panel,
                            window.deck.download_failed)

    def wait_for_update_applied(self, about_window, timeout=TIMEOUT_UPDATE_APPLY):
        """ Waits until the downloaded update has been applied.

        :param about_window: Instance of :class:`AboutWindow`.
        :param timeout: How long to wait for the update to apply. Optional,
         default to 300 seconds
        """
        Wait(self.marionette, timeout=timeout).until(
            lambda _: about_window.deck.selected_panel == about_window.deck.apply,
            message='Final wizard page has been selected.')

        # Wait for update to be staged because for update tests we modify the update
        # status file to enforce the fallback update. If we modify the file before
        # Firefox does, Firefox will override our change and we will have no fallback update.
        Wait(self.marionette, timeout=timeout).until(
            lambda _: 'applied' in self.software_update.active_update.state,
            message='Update has been applied.')
