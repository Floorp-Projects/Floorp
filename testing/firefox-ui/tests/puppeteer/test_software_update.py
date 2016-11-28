# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.api.software_update import SoftwareUpdate
from marionette import MarionetteTestCase


class TestSoftwareUpdate(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSoftwareUpdate, self).setUp()

        self.software_update = SoftwareUpdate(self.marionette)

        self.saved_mar_channels = self.software_update.mar_channels.channels
        self.saved_update_channel = self.software_update.update_channel

        self.software_update.mar_channels.channels = set(['expected', 'channels'])

    def tearDown(self):
        try:
            self.software_update.mar_channels.channels = self.saved_mar_channels
            self.software_update.update_channel = self.saved_update_channel
        finally:
            super(TestSoftwareUpdate, self).tearDown()

    def test_abi(self):
        self.assertTrue(self.software_update.ABI)

    def test_allowed(self):
        self.assertTrue(self.software_update.allowed)

    def test_build_info(self):
        build_info = self.software_update.build_info
        self.assertEqual(build_info['disabled_addons'], None)
        self.assertIn('Mozilla/', build_info['user_agent'])
        self.assertEqual(build_info['mar_channels'], set(['expected', 'channels']))
        self.assertTrue(build_info['version'])
        self.assertTrue(build_info['buildid'].isdigit())
        self.assertTrue(build_info['locale'])
        self.assertIn('force=1', build_info['update_url'])
        self.assertIn('xml', build_info['update_snippet'])
        self.assertEqual(build_info['channel'], self.software_update.update_channel)

    def test_force_fallback(self):
        status_file = os.path.join(self.software_update.staging_directory, 'update.status')

        try:
            self.software_update.force_fallback()
            with open(status_file, 'r') as f:
                content = f.read()
            self.assertEqual(content, 'failed: 6\n')
        finally:
            os.remove(status_file)

    def test_get_update_url(self):
        update_url = self.software_update.get_formatted_update_url()
        self.assertIn('Firefox', update_url)
        self.assertNotIn('force=1', update_url)
        update_url = self.software_update.get_formatted_update_url(True)
        self.assertIn('Firefox', update_url)
        self.assertIn('force=1', update_url)

    def test_os_version(self):
        self.assertTrue(self.software_update.os_version)

    def test_staging_directory(self):
        self.assertTrue(self.software_update.staging_directory)

    def test_set_update_channel(self):
        self.software_update.update_channel = 'new_channel'
        self.assertEqual(self.puppeteer.prefs.get_pref('app.update.channel', default_branch=True),
                         'new_channel')


class TestMARChannels(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestMARChannels, self).setUp()

        self.software_update = SoftwareUpdate(self.marionette)

        self.saved_mar_channels = self.software_update.mar_channels.channels
        self.software_update.mar_channels.channels = set(['expected', 'channels'])

    def tearDown(self):
        try:
            self.software_update.mar_channels.channels = self.saved_mar_channels
        finally:
            super(TestMARChannels, self).tearDown()

    def test_mar_channels_channels(self):
        self.assertEqual(self.software_update.mar_channels.channels, set(['expected', 'channels']))

    def test_mar_channels_set_channels(self):
        self.software_update.mar_channels.channels = set(['a', 'b', 'c'])
        self.assertEqual(self.software_update.mar_channels.channels, set(['a', 'b', 'c']))

    def test_mar_channels_add_channels(self):
        self.software_update.mar_channels.add_channels(set(['some', 'new', 'channels']))
        self.assertEqual(
            self.software_update.mar_channels.channels,
            set(['expected', 'channels', 'some', 'new']))

    def test_mar_channels_remove_channels(self):
        self.software_update.mar_channels.remove_channels(set(['expected']))
        self.assertEqual(self.software_update.mar_channels.channels, set(['channels']))
