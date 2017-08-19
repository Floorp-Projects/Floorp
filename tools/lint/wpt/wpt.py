# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

from mozprocess import ProcessHandler

from mozlint import result

results = []


def lint(files, config, **kwargs):
    tests_dir = os.path.join(kwargs['root'], 'testing', 'web-platform', 'tests')

    def process_line(line):
        try:
            data = json.loads(line)
        except ValueError:
            return
        data["level"] = "error"
        data["path"] = os.path.relpath(os.path.join(tests_dir, data["path"]), kwargs['root'])
        results.append(result.from_config(config, **data))

    path = os.path.join(tests_dir, "wpt")
    proc = ProcessHandler([path, "lint", "--json"] + files, env=os.environ,
                          processOutputLine=process_line)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return results
