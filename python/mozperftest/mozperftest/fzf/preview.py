# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This file is executed by fzf through the command line and needs to
work in a standalone way on any Python 3 environment.

This is why it alters PATH,making the assumption it's executed
from within a source tree. Do not add dependencies unless they
are in the source tree and added in SEARCH_PATHS.
"""
import argparse
import sys
import json
from pathlib import Path
import importlib.util


HERE = Path(__file__).parent.resolve()
SRC_ROOT = (HERE / ".." / ".." / ".." / "..").resolve()
# make sure esprima is in the path
SEARCH_PATHS = [
    ("third_party", "python", "esprima"),
]

for path in SEARCH_PATHS:
    path = Path(SRC_ROOT, *path)
    if path.exists():
        sys.path.insert(0, str(path))


def get_test_objects():
    """Loads .perftestfuzzy and returns its content.

    The cache file is produced by the main fzf script and is used
    as a way to let the preview script grab test_objects from the
    mach command
    """
    cache_file = Path(Path.home(), ".mozbuild", ".perftestfuzzy")
    with cache_file.open() as f:
        return json.loads(f.read())


def plain_display(taskfile):
    """Preview window display.

    Returns the reST summary for the perf test script.
    """
    # Lame way to catch the ScriptInfo class without loading mozperftest
    script_info = HERE / ".." / "script.py"
    spec = importlib.util.spec_from_file_location(
        name="script.py", location=str(script_info)
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    ScriptInfo = module.ScriptInfo

    with open(taskfile) as f:
        tasklist = [line.strip() for line in f]

    tags, script_name, __, location = tasklist[0].split(" ")
    script_path = Path(SRC_ROOT, location, script_name).resolve()

    for ob in get_test_objects():
        if ob["path"] == str(script_path):
            print(ScriptInfo(ob["path"]))
            return


def process_args(args):
    """Process preview arguments."""
    argparser = argparse.ArgumentParser()
    argparser.add_argument(
        "-t",
        "--tasklist",
        type=str,
        default=None,
        help="Path to temporary file containing the selected tasks",
    )
    return argparser.parse_args(args=args)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    args = process_args(args)
    plain_display(args.tasklist)


if __name__ == "__main__":
    main()
