# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import subprocess

from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozfile import which

# Error Levels
# (0, 'debug')
# (1, 'info')
# (2, 'warning')
# (3, 'error')
# (4, 'severe')

abspath = os.path.abspath(os.path.dirname(__file__))
rstcheck_requirements_file = os.path.join(abspath, "requirements.txt")

results = []

RSTCHECK_NOT_FOUND = """
Could not find rstcheck! Install rstcheck and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(
    rstcheck_requirements_file
)

RSTCHECK_INSTALL_ERROR = """
Unable to install required version of rstcheck
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(
    rstcheck_requirements_file
)

RSTCHECK_FORMAT_REGEX = re.compile(r"(.*):(.*): \(.*/([0-9]*)\) (.*)$")
IGNORE_NOT_REF_LINK_UPSTREAM_BUG = re.compile(
    r"Hyperlink target (.*) is not referenced."
)


def setup(root, **lintargs):
    virtualenv_manager = lintargs["virtualenv_manager"]
    try:
        virtualenv_manager.install_pip_requirements(
            rstcheck_requirements_file, quiet=True
        )
    except subprocess.CalledProcessError:
        print(RSTCHECK_INSTALL_ERROR)
        return 1


def get_rstcheck_binary():
    """
    Returns the path of the first rstcheck binary available
    if not found returns None
    """
    binary = os.environ.get("RSTCHECK")
    if binary:
        return binary

    return which("rstcheck")


def parse_with_split(errors):
    match = RSTCHECK_FORMAT_REGEX.match(errors)
    filename, lineno, level, message = match.groups()

    return filename, lineno, level, message


def lint(files, config, **lintargs):
    log = lintargs["log"]
    config["root"] = lintargs["root"]
    paths = expand_exclusions(files, config, config["root"])
    paths = list(paths)
    chunk_size = 50
    binary = get_rstcheck_binary()
    rstcheck_options = [
        "--ignore-language=cpp,json",
        "--ignore-roles=searchfox",
    ]

    while paths:
        cmdargs = [which("python"), binary] + rstcheck_options + paths[:chunk_size]
        log.debug("Command: {}".format(" ".join(cmdargs)))

        proc = subprocess.Popen(
            cmdargs,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ,
            universal_newlines=True,
        )
        all_errors = proc.communicate()[1]
        for errors in all_errors.split("\n"):
            if len(errors) > 1:
                filename, lineno, level, message = parse_with_split(errors)
                if not IGNORE_NOT_REF_LINK_UPSTREAM_BUG.match(message):
                    # Ignore an upstream bug
                    # https://github.com/myint/rstcheck/issues/19
                    res = {
                        "path": filename,
                        "message": message,
                        "lineno": lineno,
                        "level": "error" if int(level) >= 2 else "warning",
                    }
                    results.append(result.from_config(config, **res))
        paths = paths[chunk_size:]

    return results
