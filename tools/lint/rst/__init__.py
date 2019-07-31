# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import subprocess

from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozlint.util import pip
from mozfile import which

# Error Levels
# (0, 'debug')
# (1, 'info')
# (2, 'warning')
# (3, 'error')
# (4, 'severe')

abspath = os.path.abspath(os.path.dirname(__file__))
rstcheck_requirements_file = os.path.join(abspath, 'requirements.txt')

results = []

RSTCHECK_NOT_FOUND = """
Could not find rstcheck! Install rstcheck and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(rstcheck_requirements_file)

RSTCHECK_INSTALL_ERROR = """
Unable to install required version of rstcheck
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(rstcheck_requirements_file)


def setup(root, **lintargs):
    if not pip.reinstall_program(rstcheck_requirements_file):
        print(RSTCHECK_INSTALL_ERROR)
        return 1


def get_rstcheck_binary():
    """
    Returns the path of the first rstcheck binary available
    if not found returns None
    """
    binary = os.environ.get('RSTCHECK')
    if binary:
        return binary

    return which('rstcheck')


def parse_with_split(errors):

    filtered_output = errors.split(":")
    filename = filtered_output[0]
    lineno = filtered_output[1]
    idx = filtered_output[2].index(") ")
    level = filtered_output[2][0:idx].split("/")[1]
    message = filtered_output[2][idx+2:].split("\n")[0]

    return filename, lineno, level, message


def lint(files, config, **lintargs):

    config['root'] = lintargs['root']
    paths = expand_exclusions(files, config, config['root'])
    paths = list(paths)
    chunk_size = 50
    binary = get_rstcheck_binary()

    while paths:
        cmdargs = [
            binary,
        ] + paths[:chunk_size]
        proc = subprocess.Popen(
            cmdargs, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ
            )
        all_errors = proc.communicate()[1]
        for errors in all_errors.split("\n"):
            if len(errors) > 1:
                filename, lineno, level, message = parse_with_split(errors)
                res = {
                    'path': filename,
                    'message': message,
                    'lineno': lineno,
                    'level': "error" if int(level) >= 2 else "warning",
                }
                results.append(result.from_config(config, **res))
        paths = paths[chunk_size:]

    return results
