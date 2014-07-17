# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "marionette_desktop_options": [
        "--type=%(type)s",
        "--binary=%(binary)s",
        "--address=%(address)s",
    ],
    "marionette_emulator_options": [
        "--type=%(type)s",
        "--logcat-dir=%(logcat_dir)s",
        "--emulator=%(emulator)s",
        "--homedir=%(homedir)s",
    ],
    "webapi_emulator_options": [
        "--type=%(type)s",
        "--symbols-path=%(symbols_path)s",
        "--logcat-dir=%(logcat_dir)s",
        "--emulator=%(emulator)s",
        "--homedir=%(homedir)s",
    ],
    # This combination is not currently run.
    "webapi_desktop_options": [
    ],
    "gaiatest_emulator_options": [
        "--restart",
        "--timeout=%(timeout)s",
        "--type=%(type)s",
        "--testvars=%(testvars)s",
        "--profile=%(profile)s",
        "--symbols-path=%(symbols_path)s",
        "--xml-output=%(xml_output)s",
        "--html-output=%(html_output)s",
        "--logcat-dir=%(logcat_dir)s",
        "--emulator=%(emulator)s",
        "--homedir=%(homedir)s",
    ],
    "gaiatest_desktop_options": [
        "--restart",
        "--timeout=%(timeout)s",
        "--type=%(type)s",
        "--testvars=%(testvars)s",
        "--profile=%(profile)s",
        "--symbols-path=%(symbols_path)s",
        "--xml-output=%(xml_output)s",
        "--html-output=%(html_output)s",
        "--binary=%(binary)s",
        "--address=%(address)s",
    ],
}
