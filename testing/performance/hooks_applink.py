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

app_options = {
    "org.mozilla.geckoview_example": [
        ("firefox.android.activity", "'org.mozilla.geckoview_example.GeckoViewActivity'")
    ],
    "org.mozilla.fennec_aurora": [
        ("firefox.android.activity", "'org.mozilla.fenix.IntentReceiverActivity'")
    ],
    "org.mozilla.firefox": [
        ("firefox.android.activity", "'org.mozilla.gecko.BrowserApp'")
    ]
}


def before_runs(env, **kw):
    add_options(env, common_options)
    add_options(env, app_options[env.get_arg("android-app-name")])
