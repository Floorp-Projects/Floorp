# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from firefox_ui_harness.testcases import FirefoxTestCase
from marionette_driver import Wait


class TestSafeBrowsingInitialDownload(FirefoxTestCase):

    file_extensions = [
        'pset',
        'sbstore',
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
        'browser.safebrowsing.provider.google.nextupdatetime': 1,
        'browser.safebrowsing.provider.mozilla.nextupdatetime': 1,
    }

    prefs_safebrowsing = {
        'browser.safebrowsing.blockedURIs.enabled': True,
        'browser.safebrowsing.downloads.enabled': True,
        'browser.safebrowsing.phishing.enabled': True,
        'browser.safebrowsing.malware.enabled': True,
        'privacy.trackingprotection.enabled': True,
        'privacy.trackingprotection.pbmode.enabled': True,
    }

    def get_safebrowsing_files(self):
        files = []
        for pref_name in self.prefs_download_lists:
            base_names = self.marionette.get_pref(pref_name).split(',')
            for ext in self.file_extensions:
                files.extend(['{file}.{ext}'.format(file=f, ext=ext) for f in base_names if f])

        return set(sorted(files))

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Force the preferences for the new profile
        enforce_prefs = self.prefs_safebrowsing
        enforce_prefs.update(self.prefs_provider_update_time)
        self.marionette.enforce_gecko_prefs(enforce_prefs)

        self.safebrowsing_path = os.path.join(self.marionette.instance.profile.profile,
                                              'safebrowsing')
        self.safebrowsing_files = self.get_safebrowsing_files()

    def tearDown(self):
        try:
            # Restart with a fresh profile
            self.restart(clean=True)
        finally:
            FirefoxTestCase.tearDown(self)

    def test_safe_browsing_initial_download(self):
        def check_downloaded(_):
            return reduce(lambda state, pref: state and int(self.marionette.get_pref(pref)) != 1,
                          self.prefs_provider_update_time.keys(), True)

        try:
            Wait(self.marionette, timeout=60).until(
                check_downloaded, message='Not all safebrowsing files have been downloaded')
        finally:
            self.assertSetEqual(self.safebrowsing_files, set(os.listdir(self.safebrowsing_path)))
