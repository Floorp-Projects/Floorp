# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

from mozprocess import ProcessHandler

from mozlint import result

top_src_dir = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
tests_dir = os.path.join(top_src_dir, "testing", "web-platform", "tests")

results = []


def process_line(line):
    try:
        data = json.loads(line)
    except ValueError:
        return
    data["level"] = "error"
    data["path"] = os.path.relpath(os.path.join(tests_dir, data["path"]), top_src_dir)
    results.append(result.from_linter(LINTER, **data))


def run_process():
    path = os.path.join(tests_dir, "lint")
    proc = ProcessHandler([path, "--json"], env=os.environ,
                          processOutputLine=process_line)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()


def lint(files, **kwargs):
    run_process()
    return results


LINTER = {
    'name': "wpt",
    'description': "web-platform-tests lint",
    'include': [
        'testing/web-platform/tests',
    ],
    'exclude': [],
    'type': 'external',
    'payload': lint,
}
