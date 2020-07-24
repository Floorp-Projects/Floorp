# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
High-level interface for translating `metrics.yaml` into other formats.
"""

from pathlib import Path
import os
import shutil
import sys
import tempfile

from . import lint
from . import parser
from . import kotlin
from . import markdown
from . import swift
from . import util


# Each outputter in the table has the following keys:
# - "output_func": the main function of the outputter, the one which
#   does the actual translation.
# - "clear_output_dir": a flag to clear the target directory before moving there
#   the generated files.
OUTPUTTERS = {
    "kotlin": {
        "output_func": kotlin.output_kotlin,
        "clear_output_dir": True,
        "extensions": ["*.kt"],
    },
    "markdown": {"output_func": markdown.output_markdown, "clear_output_dir": False},
    "swift": {
        "output_func": swift.output_swift,
        "clear_output_dir": True,
        "extensions": ["*.swift"],
    },
}


def translate(input_filepaths, output_format, output_dir, options={}, parser_config={}):
    """
    Translate the files in `input_filepaths` to the given `output_format` and
    put the results in `output_dir`.

    :param input_filepaths: list of paths to input metrics.yaml files
    :param output_format: the name of the output formats
    :param output_dir: the path to the output directory
    :param options: dictionary of options. The available options are backend
        format specific.
    :param parser_config: A dictionary of options that change parsing behavior.
        See `parser.parse_metrics` for more info.
    """
    if output_format not in OUTPUTTERS:
        raise ValueError("Unknown output format '{}'".format(output_format))

    all_objects = parser.parse_objects(input_filepaths, parser_config)

    if util.report_validation_errors(all_objects):
        return 1

    if lint.lint_metrics(all_objects.value, parser_config):
        print(
            "NOTE: These warnings will become errors in a future release of Glean.",
            file=sys.stderr,
        )

    # allow_reserved is also relevant to the translators, so copy it there
    if parser_config.get("allow_reserved"):
        options["allow_reserved"] = True

    # Write everything out to a temporary directory, and then move it to the
    # real directory, for transactional integrity.
    with tempfile.TemporaryDirectory() as tempdir:
        tempdir_path = Path(tempdir)
        OUTPUTTERS[output_format]["output_func"](
            all_objects.value, tempdir_path, options
        )

        if OUTPUTTERS[output_format]["clear_output_dir"]:
            if output_dir.is_file():
                output_dir.unlink()
            elif output_dir.is_dir():
                for extensions in OUTPUTTERS[output_format]["extensions"]:
                    for filepath in output_dir.glob(extensions):
                        filepath.unlink()
                if len(list(output_dir.iterdir())):
                    print("Extra contents found in '{}'.".format(output_dir))

        # We can't use shutil.copytree alone if the directory already exists.
        # However, if it doesn't exist, make sure to create one otherwise
        # shutil.copy will fail.
        os.makedirs(str(output_dir), exist_ok=True)
        for filename in tempdir_path.glob("*"):
            shutil.copy(str(filename), str(output_dir))

    return 0
