# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "suite_definitions": {
        "gaiatest_desktop": {
            "options": [
                "--restart",
                "--timeout=%(timeout)s",
                "--type=%(type)s",
                "--testvars=%(testvars)s",
                "--profile=%(profile)s",
                "--symbols-path=%(symbols_path)s",
                "--gecko-log=%(gecko_log)s",
                "--xml-output=%(xml_output)s",
                "--html-output=%(html_output)s",
                "--log-raw=%(raw_log_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "gaiatest_emulator": {
            "options": [
                "--restart",
                "--timeout=%(timeout)s",
                "--type=%(type)s",
                "--testvars=%(testvars)s",
                "--profile=%(profile)s",
                "--symbols-path=%(symbols_path)s",
                "--xml-output=%(xml_output)s",
                "--html-output=%(html_output)s",
                "--log-raw=%(raw_log_file)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "marionette_desktop": {
            "options": [
                "--type=%(type)s",
                "--log-raw=%(raw_log_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "marionette_emulator": {
            "options": [
                "--type=%(type)s",
                "--log-raw=%(raw_log_file)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "webapi_desktop": {
            "options": [],
            "run_filename": "",
            "testsdir": ""
        },
        "webapi_emulator": {
            "options": [
                "--type=%(type)s",
                "--log-raw=%(raw_log_file)s",
                "--symbols-path=%(symbols_path)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s"
            ],
            "run_filename": "",
            "testsdir": ""
        }
    }
}