# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "suite_definitions": {
        "mochitest": {
            "options": [
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--profile=%(gaia_profile)s",
                "--app=%(application)s",
                "--desktop",
                "--utility-path=%(utility_path)s",
                "--certificate-path=%(cert_path)s",
                "--symbols-path=%(symbols_path)s",
                "--browser-arg=%(browser_arg)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s"
            ],
            "run_filename": "runtestsb2g.py",
            "testsdir": "mochitest"
        },
        "reftest": {
            "options": [
                "--desktop",
                "--profile=%(gaia_profile)s",
                "--appname=%(application)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--browser-arg=%(browser_arg)s",
                "--symbols-path=%(symbols_path)s",
                "%(test_manifest)s"
            ],
            "run_filename": "runreftestsb2g.py",
            "testsdir": "reftest"
        }
    }
}
