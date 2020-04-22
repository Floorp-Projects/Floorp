# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozperftest.browser.browsertime.runner import BrowsertimeRunner  # noqa


def add_option(env, name, value):
    options = env.get_arg("browsertime-extra-options", "")
    options += ",%s=%s" % (name, value)
    env.set_arg("browsertime-extra-options", options)


def add_options(env, options):
    for name, value in options:
        add_option(env, name, value)
