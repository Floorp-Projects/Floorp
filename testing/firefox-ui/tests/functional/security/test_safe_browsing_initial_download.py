# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from firefox_puppeteer import PuppeteerMixin
from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class TestSafeBrowsingInitialDownload(PuppeteerMixin, MarionetteTestCase):

    v2_file_extensions = [
        'pset',
        'sbstore',
    ]

    v4_file_extensions = [
        'pset',
        'metadata',
    ]

    prefs_download_lists = [
        'urlclassifier.blockedTable',
        'urlclassifier.downloadAllowTable',
        'urlclassifier.downloadBlockTable',
        'urlclassifier.malwareTable',
        'urlclassifier.phishTable',
        'urlclassifier.trackingTable',
        'urlclassifier.trackingWhitelistTable',
    ]

    prefs_provider_update_time = {
        # Force an immediate download of the safebrowsing files
        # Bug 1330253 - Leave the next line disabled until we have google API key
        #               on the CI machines.
        # 'browser.safebrowsing.provider.google4.nextupdatetime': 1,
        'browser.safebrowsing.provider.google.nextupdatetime': 1,
        'browser.safebrowsing.provider.mozilla.nextupdatetime': 1,
    }

    prefs_safebrowsing = {
        'browser.safebrowsing.debug': True,
        'browser.safebrowsing.blockedURIs.enabled': True,
        'browser.safebrowsing.downloads.enabled': True,
        'browser.safebrowsing.phishing.enabled': True,
        'browser.safebrowsing.malware.enabled': True,
        'privacy.trackingprotection.enabled': True,
        'privacy.trackingprotection.pbmode.enabled': True,
        'urlclassifier.malwareTable':
        'goog-malware-shavar,goog-unwanted-shavar,test-malware-simple,test-unwanted-simple',
        'urlclassifier.phishTable': 'googpub-phish-shavar,test-phish-simple',
    }

    def get_safebrowsing_files(self, is_v4):
        files = []

        if is_v4:
            my_file_extensions = self.v4_file_extensions
        else:  # v2
            my_file_extensions = self.v2_file_extensions

        for pref_name in self.prefs_download_lists:
            base_names = self.marionette.get_pref(pref_name).split(',')
            for ext in my_file_extensions:
                files.extend(['{name}.{ext}'.format(name=f, ext=ext)
                              for f in base_names if f and f.endswith('-proto') == is_v4])

        return set(sorted(files))

    def setUp(self):
        super(TestSafeBrowsingInitialDownload, self).setUp()

        # Force the preferences for the new profile
        enforce_prefs = self.prefs_safebrowsing
        enforce_prefs.update(self.prefs_provider_update_time)
        self.marionette.enforce_gecko_prefs(enforce_prefs)

        self.safebrowsing_path = os.path.join(self.marionette.instance.profile.profile,
                                              'safebrowsing')
        self.safebrowsing_v2_files = self.get_safebrowsing_files(False)
        # Bug 1330253 - Leave the next line disabled until we have google API key
        #               on the CI machines.
        # self.safebrowsing_v4_files = self.get_safebrowsing_files(True)
        self.safebrowsing_v4_files = []

    def tearDown(self):
        try:
            # Restart with a fresh profile
            self.restart(clean=True)
        finally:
            super(TestSafeBrowsingInitialDownload, self).tearDown()

    def test_safe_browsing_initial_download(self):
        def check_downloaded(_):
            return reduce(lambda state, pref: state and int(self.marionette.get_pref(pref)) != 1,
                          self.prefs_provider_update_time.keys(), True)

        try:
            Wait(self.marionette, timeout=60).until(
                check_downloaded, message='Not all safebrowsing files have been downloaded')
        finally:
            files_on_disk_toplevel = os.listdir(self.safebrowsing_path)
            for f in self.safebrowsing_v2_files:
                self.assertIn(f, files_on_disk_toplevel)

            if len(self.safebrowsing_v4_files) > 0:
                files_on_disk_google4 = os.listdir(os.path.join(self.safebrowsing_path, 'google4'))
                for f in self.safebrowsing_v4_files:
                    self.assertIn(f, files_on_disk_google4)
