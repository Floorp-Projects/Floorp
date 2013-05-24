# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""The config defined in this module is read by mozharness, which is a
tool used to run many of the tests reporting to TBPL. This file is
primarily useful for pushing custom test harness configurations to try.

For example you can define a custom mochitest configuration by adding:
    "mochitest_options": [
        "--appname=%(binary_path)s", "--utility-path=tests/bin",
        "--extra-profile-file=tests/bin/plugins", "--symbols-path=%(symbols_path)s",
        "--certificate-path=tests/certs", "--autorun", "--close-when-done",
        "--console-level=INFO", "--setpref=webgl.force-enabled=true"
    ],

Be warned that these values will be picked up by all platforms and changing them
may result in unexpected behaviour. For example, the above will break B2G
mochitests.

You must also provide the complete command line to avoid errors. The official
configuration files containing the default values live in:
    https://hg.mozilla.org/build/mozharness/configs
"""

config = {
    # Add custom mozharness config options here
}
