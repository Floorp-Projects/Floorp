# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a template config file for marionette production on Windows.
config = {
    # marionette options
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.toml",
    "virtualenv_path": "venv",
    "exes": {
        "python": "c:/mozilla-build/python27/python",
        "hg": "c:/mozilla-build/hg/hg",
    },
    "default_actions": [
        "clobber",
        "download-and-extract",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "suite_definitions": {
        "marionette_desktop": {
            "options": [
                "-vv",
                "--log-errorsummary=%(error_summary_file)s",
                "--log-html=%(html_report_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--symbols-path=%(symbols_path)s",
            ],
            "run_filename": "",
            "testsdir": "marionette",
        },
    },
}
