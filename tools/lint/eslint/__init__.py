# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import re
import signal
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "eslint"))
from eslint import setup_helper

from mozbuild.nodeutil import find_node_executable

from mozprocess import ProcessHandler

from mozlint import result

ESLINT_ERROR_MESSAGE = """
An error occurred running eslint. Please check the following error messages:

{}
""".strip()

ESLINT_NOT_FOUND_MESSAGE = """
Could not find eslint!  We looked at the --binary option, at the ESLINT
environment variable, and then at your local node_modules path. Please Install
eslint and needed plugins with:

mach eslint --setup

and try again.
""".strip()


def setup(root, **lintargs):
    setup_helper.set_project_root(root)

    if not setup_helper.check_node_executables_valid():
        return 1

    return setup_helper.eslint_maybe_setup()


def lint(paths, config, binary=None, fix=None, setup=None, **lintargs):
    """Run eslint."""
    setup_helper.set_project_root(lintargs['root'])
    module_path = setup_helper.get_project_root()

    # Valid binaries are:
    #  - Any provided by the binary argument.
    #  - Any pointed at by the ESLINT environmental variable.
    #  - Those provided by |mach lint --setup|.

    if not binary:
        binary, _ = find_node_executable()

    if not binary:
        print(ESLINT_NOT_FOUND_MESSAGE)
        return 1

    extra_args = lintargs.get('extra_args') or []
    exclude_args = []
    for path in config.get('exclude', []):
        exclude_args.extend(['--ignore-pattern', os.path.relpath(path, lintargs['root'])])

    cmd_args = [binary,
                os.path.join(module_path, "node_modules", "eslint", "bin", "eslint.js"),
                # This keeps ext as a single argument.
                '--ext', '[{}]'.format(','.join(config['extensions'])),
                '--format', 'json',
                ] + extra_args + exclude_args + paths

    # eslint requires that --fix be set before the --ext argument.
    if fix:
        cmd_args.insert(2, '--fix')

    shell = False
    if os.environ.get('MSYSTEM') in ('MINGW32', 'MINGW64'):
        # The eslint binary needs to be run from a shell with msys
        shell = True

    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = ProcessHandler(cmd_args, env=os.environ, stream=None, shell=shell)
    proc.run()
    signal.signal(signal.SIGINT, orig)

    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()
        return []

    if not proc.output:
        return []  # no output means success

    try:
        jsonresult = json.loads(proc.output[0])
    except ValueError:
        output = "\n".join(proc.output)
        if re.search(r'No files matching the pattern "(.*)" were found.', output):
            print("warning: no files to lint (eslint)")
            return []

        print(ESLINT_ERROR_MESSAGE.format(output))
        return 1

    results = []
    for obj in jsonresult:
        errors = obj['messages']

        for err in errors:
            err.update({
                'hint': err.get('fix'),
                'level': 'error' if err['severity'] == 2 else 'warning',
                'lineno': err.get('line') or 0,
                'path': obj['filePath'],
                'rule': err.get('ruleId'),
            })
            results.append(result.from_config(config, **err))

    return results
