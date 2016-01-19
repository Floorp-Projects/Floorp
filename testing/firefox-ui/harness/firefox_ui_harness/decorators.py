# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import SkipTest
import os


def skip_under_xvfb(target):
    def wrapper(self, *args, **kwargs):
        if os.environ.get('MOZ_XVFB'):
            raise SkipTest("Skipping due to running under xvfb")
        return target(self, *args, **kwargs)
    return wrapper
