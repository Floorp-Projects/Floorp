# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import signal
import setup_helper

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


def lint(paths, binary=None, fix=None, setup=None, **lintargs):
    """Run eslint."""
    global project_root
    setup_helper.set_project_root(lintargs['root'])

    module_path = setup_helper.get_project_root()

    if not setup_helper.check_node_executables_valid():
        return 1

    if setup:
        return setup_helper.eslint_setup()

    if setup_helper.eslint_module_needs_setup():
        setup_helper.eslint_setup()

    # Valid binaries are:
    #  - Any provided by the binary argument.
    #  - Any pointed at by the ESLINT environmental variable.
    #  - Those provided by mach eslint --setup.
    #
    #  eslint --setup installs some mozilla specific plugins and installs
    #  all node modules locally. This is the preferred method of
    #  installation.

    if not binary:
        binary = os.environ.get('ESLINT', None)

        if not binary:
            binary = os.path.join(module_path, "node_modules", ".bin", "eslint")
            if not os.path.isfile(binary):
                binary = None

    if not binary:
        print(ESLINT_NOT_FOUND_MESSAGE)
        return 1

    extra_args = lintargs.get('extra_args') or []
    cmd_args = [binary,
                # Enable the HTML plugin.
                # We can't currently enable this in the global config file
                # because it has bad interactions with the SublimeText
                # ESLint plugin (bug 1229874).
                '--plugin', 'html',
                # This keeps ext as a single argument.
                '--ext', '[{}]'.format(','.join(setup_helper.EXTENSIONS)),
                '--format', 'json',
                ] + extra_args + paths

    # eslint requires that --fix be set before the --ext argument.
    if fix:
        cmd_args.insert(1, '--fix')

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
        print(ESLINT_ERROR_MESSAGE.format("\n".join(proc.output)))
        return 1

    results = []
    for obj in jsonresult:
        errors = obj['messages']

        for err in errors:
            err.update({
                'hint': err.get('fix'),
                'level': 'error' if err['severity'] == 2 else 'warning',
                'lineno': err.get('line'),
                'path': obj['filePath'],
                'rule': err.get('ruleId'),
            })
            results.append(result.from_linter(LINTER, **err))

    return results


LINTER = {
    'name': "eslint",
    'description': "JavaScript linter",
    # ESLint infra handles its own path filtering, so just include cwd
    'include': ['.'],
    'exclude': [],
    'extensions': setup_helper.EXTENSIONS,
    'type': 'external',
    'payload': lint,
}
