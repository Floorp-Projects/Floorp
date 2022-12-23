# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette_harness import BaseMarionetteTestRunner, MarionetteTestCase


class FirefoxUITestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        super(FirefoxUITestRunner, self).__init__(**kwargs)

        # select the appropriate GeckoInstance
        self.app = "fxdesktop"

        # low-noise log messages useful in tests
        # TODO: should be moved to individual tests once bug 1386810
        # is fixed
        moz_log = ""
        if "MOZ_LOG" in os.environ:
            moz_log = os.environ["MOZ_LOG"]
        if len(moz_log) > 0:
            moz_log += ","
        moz_log += "UrlClassifierStreamUpdater:1"
        os.environ["MOZ_LOG"] = moz_log

        self.test_handlers = [MarionetteTestCase]
