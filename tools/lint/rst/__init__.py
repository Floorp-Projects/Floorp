# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import subprocess
import json

from mozlint import result

# Error Levels
# (0, 'debug')
# (1, 'info')
# (2, 'warning')
# (3, 'error')
# (4, 'severe')

abspath = os.path.abspath(os.path.dirname(__file__))
rstlint_requirements_file = os.path.join(abspath, 'requirements.txt')

RSTLINT_INSTALL_ERROR = """
Unable to install required version of restructuredtext_lint and docutils
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(rstlint_requirements_file)


def pip_installer(*args):
    """
    Helper function that runs pip with subprocess
    """
    try:
        subprocess.check_output(['pip'] + list(args),
                                stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError as e:
        print(e.output)
        return False


def install_rstlint():
    """
    Try to install rstlint and docutils at the target version, returns True on success
    otherwise prints the otuput of the pip command and returns False
    """
    if pip_installer('install', '-U',
                     '--require-hashes', '-r',
                     rstlint_requirements_file):
        return True

    return False


def lint(files, config, **lintargs):

    if not install_rstlint():
        print(RSTLINT_INSTALL_ERROR)
        return 1

    config = config.copy()
    config['root'] = lintargs['root']

    cmdargs = [
        'rst-lint',
        '--format=json',
    ] + files

    proc = subprocess.Popen(cmdargs, stdout=subprocess.PIPE, env=os.environ)
    output = proc.communicate()[0]

    # all passed
    if not output:
        return []

    results = []

    for i in (json.loads(output)):

        # create a dictionary to append with mozlint.results
        res = {
            'path': i['source'],
            'message': i['message'],
            'lineno': i['line'],
            'level': i['type'].lower(),
            'rule': i['level'],
        }

        results.append(result.from_config(config, **res))

    return results
