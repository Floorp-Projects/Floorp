# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import platform
import subprocess
import sys

import mozfile
import mozpack.path as mozpath

from mozlint import result
from mozlint.pathutils import expand_exclusions

here = os.path.abspath(os.path.dirname(__file__))
FLAKE8_REQUIREMENTS_PATH = os.path.join(here, "flake8_requirements.txt")

FLAKE8_NOT_FOUND = """
Could not find flake8! Install flake8 and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(
    FLAKE8_REQUIREMENTS_PATH
)


FLAKE8_INSTALL_ERROR = """
Unable to install correct version of flake8
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(
    FLAKE8_REQUIREMENTS_PATH
)

LINE_OFFSETS = {
    # continuation line under-indented for hanging indent
    "E121": (-1, 2),
    # continuation line missing indentation or outdented
    "E122": (-1, 2),
    # continuation line over-indented for hanging indent
    "E126": (-1, 2),
    # continuation line over-indented for visual indent
    "E127": (-1, 2),
    # continuation line under-indented for visual indent
    "E128": (-1, 2),
    # continuation line unaligned for hanging indend
    "E131": (-1, 2),
    # expected 1 blank line, found 0
    "E301": (-1, 2),
    # expected 2 blank lines, found 1
    "E302": (-2, 3),
}
"""Maps a flake8 error to a lineoffset tuple.

The offset is of the form (lineno_offset, num_lines) and is passed
to the lineoffset property of an `Issue`.
"""


def default_bindir():
    # We use sys.prefix to find executables as that gets modified with
    # virtualenv's activate_this.py, whereas sys.executable doesn't.
    if platform.system() == "Windows":
        return os.path.join(sys.prefix, "Scripts")
    else:
        return os.path.join(sys.prefix, "bin")


class NothingToLint(Exception):
    """Exception used to bail out of flake8's internals if all the specified
    files were excluded.
    """


def setup(root, **lintargs):
    virtualenv_manager = lintargs["virtualenv_manager"]
    try:
        virtualenv_manager.install_pip_requirements(
            FLAKE8_REQUIREMENTS_PATH, quiet=True
        )
    except subprocess.CalledProcessError:
        print(FLAKE8_INSTALL_ERROR)
        return 1


def lint(paths, config, **lintargs):

    root = lintargs["root"]
    virtualenv_bin_path = lintargs.get("virtualenv_bin_path")
    config_path = os.path.join(root, ".flake8")

    results = run(paths, config, **lintargs)
    fixed = 0

    if lintargs.get("fix"):
        # fix and run again to count remaining issues
        fixed = len(results)
        fix_cmd = [
            os.path.join(virtualenv_bin_path or default_bindir(), "autopep8"),
            "--global-config",
            config_path,
            "--in-place",
            "--recursive",
        ]

        if config.get("exclude"):
            fix_cmd.extend(["--exclude", ",".join(config["exclude"])])

        subprocess.call(fix_cmd + paths)
        results = run(paths, config, **lintargs)

        fixed = fixed - len(results)

    return {"results": results, "fixed": fixed}


def run(paths, config, **lintargs):
    from flake8.main.application import Application
    from flake8 import __version__ as flake8_version

    log = lintargs["log"]
    root = lintargs["root"]
    config_path = os.path.join(root, ".flake8")

    # Run flake8.
    app = Application()
    log.debug("flake8 version={}".format(flake8_version))

    output_file = mozfile.NamedTemporaryFile(mode="r")
    flake8_cmd = [
        "--config",
        config_path,
        "--output-file",
        output_file.name,
        "--format",
        '{"path":"%(path)s","lineno":%(row)s,'
        '"column":%(col)s,"rule":"%(code)s","message":"%(text)s"}',
        "--filename",
        ",".join(["*.{}".format(e) for e in config["extensions"]]),
    ]
    log.debug("Command: {}".format(" ".join(flake8_cmd)))

    orig_make_file_checker_manager = app.make_file_checker_manager

    def wrap_make_file_checker_manager(self):
        """Flake8 is very inefficient when it comes to applying exclusion
        rules, using `expand_exclusions` to turn directories into a list of
        relevant python files is an order of magnitude faster.

        Hooking into flake8 here also gives us a convenient place to merge the
        `exclude` rules specified in the root .flake8 with the ones added by
        tools/lint/mach_commands.py.
        """
        # Ignore exclude rules if `--no-filter` was passed in.
        config.setdefault("exclude", [])
        if lintargs.get("use_filters", True):
            config["exclude"].extend(map(mozpath.normpath, self.options.exclude))

        # Since we use the root .flake8 file to store exclusions, we haven't
        # properly filtered the paths through mozlint's `filterpaths` function
        # yet. This mimics that though there could be other edge cases that are
        # different. Maybe we should call `filterpaths` directly, though for
        # now that doesn't appear to be necessary.
        filtered = [
            p for p in paths if not any(p.startswith(e) for e in config["exclude"])
        ]

        self.options.filenames = self.options.filenames + list(
            expand_exclusions(filtered, config, root)
        )

        if not self.options.filenames:
            raise NothingToLint
        return orig_make_file_checker_manager()

    app.make_file_checker_manager = wrap_make_file_checker_manager.__get__(
        app, Application
    )

    # Make sure to run from repository root so exclusions are joined to the
    # repository root and not the current working directory.
    oldcwd = os.getcwd()
    os.chdir(root)
    try:
        app.run(flake8_cmd)
    except NothingToLint:
        pass
    finally:
        os.chdir(oldcwd)

    results = []

    def process_line(line):
        # Escape slashes otherwise JSON conversion will not work
        line = line.replace("\\", "\\\\")
        try:
            res = json.loads(line)
        except ValueError:
            print("Non JSON output from linter, will not be processed: {}".format(line))
            return

        if res.get("code") in LINE_OFFSETS:
            res["lineoffset"] = LINE_OFFSETS[res["code"]]

        results.append(result.from_config(config, **res))

    list(map(process_line, output_file.readlines()))
    return results
