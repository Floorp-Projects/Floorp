# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re

from firefox_ui_harness.testcases import FirefoxTestCase
from marionette_driver import Wait


class TestSafeBrowsingInitialDownload(FirefoxTestCase):

    test_data = [{
        'platforms': ['linux', 'windows_nt', 'darwin'],
        'files': [
            # Phishing
            r'goog-badbinurl-shavar.pset',
            r'goog-badbinurl-shavar.sbstore',
            r'goog-malware-shavar.pset',
            r'goog-malware-shavar.sbstore',
            r'goog(pub)?-phish-shavar.pset',
            r'goog(pub)?-phish-shavar.sbstore',
            r'goog-unwanted-shavar.pset',
            r'goog-unwanted-shavar.sbstore',

            # Tracking Protections
            r'base-track-digest256.pset',
            r'base-track-digest256.sbstore',
            r'mozstd-trackwhite-digest256.pset',
            r'mozstd-trackwhite-digest256.sbstore'
        ],
    }, {
        'platforms': ['windows_nt'],
        'files': [
            r'goog-downloadwhite-digest256.pset',
            r'goog-downloadwhite-digest256.sbstore'
        ]
    },
    ]

    browser_prefs = {
        'browser.safebrowsing.downloads.enabled': 'true',
        'browser.safebrowsing.phishing.enabled': 'true',
        'browser.safebrowsing.malware.enabled': 'true',
        'browser.safebrowsing.provider.google.nextupdatetime': 1,
        'browser.safebrowsing.provider.mozilla.nextupdatetime': 1,
        'privacy.trackingprotection.enabled': 'true',
        'privacy.trackingprotection.pbmode.enabled': 'true',
    }

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Set Browser Preferences
        self.marionette.enforce_gecko_prefs(self.browser_prefs)

        # Get safebrowsing path where downloaded data gets stored
        self.sb_files_path = os.path.join(self.marionette.instance.profile.profile, 'safebrowsing')

    def tearDown(self):
        try:
            self.restart(clean=True)
        finally:
            FirefoxTestCase.tearDown(self)

    def test_safe_browsing_initial_download(self):
        wait = Wait(self.marionette, timeout=self.browser.timeout_page_load)

        for data in self.test_data:
            if self.platform not in data['platforms']:
                continue
            for item in data['files']:
                wait.until(
                    lambda _: [f for f in os.listdir(self.sb_files_path) if re.search(item, f)],
                    message='Safe Browsing File: {} not found!'.format(item))
