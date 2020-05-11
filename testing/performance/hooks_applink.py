# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.browser.browsertime import add_options

url = "'https://www.example.com'"

common_options = [("processStartTime", "true"),
                  ("firefox.disableBrowsertimeExtension", "true"),
                  ("firefox.android.intentArgument", "'-a'"),
                  ("firefox.android.intentArgument", "'android.intent.action.VIEW'"),
                  ("firefox.android.intentArgument", "'-d'"),
                  ("firefox.android.intentArgument", url)]


def before_runs(env, **kw):
    add_options(env, common_options)
