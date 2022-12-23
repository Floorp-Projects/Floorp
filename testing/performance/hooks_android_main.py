# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.test.browsertime import add_options
from mozperftest.test.browsertime.runner import NodeException

common_options = [
    ("firefox.disableBrowsertimeExtension", "true"),
    # The webdriver session is not created in the MAIN test so reduce the timeout
    # Bug 1640638
    ("timeouts.browserStart", "20000"),
    ("browserRestartTries", "1"),
    # Explicitly disable the Onboarding flow
    ("firefox.android.intentArgument", "'--ez'"),
    ("firefox.android.intentArgument", "'performancetest'"),
    ("firefox.android.intentArgument", "'true'"),
]


def on_exception(env, layer, exc):
    if not isinstance(exc, NodeException):
        raise exc
    return True


def logcat_processor():
    pass


def before_runs(env, **kw):
    add_options(env, common_options)
