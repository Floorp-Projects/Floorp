# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os


def lint(files, logger, **kwargs):
    wpt_dir = os.path.join(kwargs["root"], "testing", "web-platform")
    manifestupdate = imp.load_source("manifestupdate",
                                     os.path.join(wpt_dir, "manifestupdate.py"))
    manifestupdate.update(logger, wpt_dir, True)


LINTER = {
    'name': "wpt_manifest",
    'description': "web-platform-tests manifest lint",
    'include': [
        'testing/web-platform/tests',
        'testing/web-platform/mozilla/tests',
    ],
    'exclude': [],
    'type': 'structured_log',
    'payload': lint,
}
