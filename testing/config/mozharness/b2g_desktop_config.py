# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "mochitest_options": [
        "--console-level=INFO", "%(test_manifest)s",
        "--total-chunks=%(total_chunks)s", "--this-chunk=%(this_chunk)s",
        "--profile=%(gaia_profile)s", "--app=%(application)s", "--desktop",
        "--utility-path=%(utility_path)s", "--certificate-path=%(cert_path)s",
        "--symbols-path=%(symbols_path)s",
    ],

    "reftest_options": [
        "--desktop", "--profile=%(gaia_profile)s", "--appname=%(application)s",
        "--symbols-path=%(symbols_path)s", "%(test_manifest)s",
    ]
}
