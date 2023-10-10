# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import configparser
import os
import re
import subprocess
import sys

here = os.path.abspath(os.path.dirname(__file__))

local_requirements = {
    b"mozinfo": "testing/mozbase/mozinfo",
    b"mozlog": "testing/mozbase/mozlog",
    b"mozdebug": "testing/mozbase/mozdebug",
    b"marionette_driver": "testing/marionette/client/",
    b"mozprofile": "testing/mozbase/mozprofile",
    b"mozprocess": "testing/mozbase/mozprocess",
    b"mozcrash": "testing/mozbase/mozcrash",
    b"mozrunner": "testing/mozbase/mozrunner",
    b"mozleak": "testing/mozbase/mozleak",
    b"mozversion": "testing/mozbase/mozversion",
}

requirements_re = re.compile(rb"(%s)[^\w]" % b"|".join(local_requirements.keys()))


class ReplaceRequirements(object):
    def __init__(self, top_src_path, tox_path):
        self.top_src_path = top_src_path
        self.tox_path = tox_path
        self.file_cache = {}

    def __enter__(self):
        self.file_cache = {}
        deps = self.read_deps()
        for dep in deps:
            self.replace_path(dep)

    def __exit__(self, *args, **kwargs):
        for path, data in self.file_cache.items():
            with open(path, "wb") as f:
                f.write(data)

    def read_deps(self):
        rv = []
        parser = configparser.ConfigParser()
        path = os.path.join(self.tox_path, "tox.ini")
        with open(path) as f:
            parser.read_file(f)
        deps = parser.get("testenv", "deps")
        dep_re = re.compile("(?:.*:\s*)?-r(.*)")

        # This can break if we start using more features of tox
        for dep in deps.splitlines():
            m = dep_re.match(dep)
            if m:
                rv.append(m.group(1).replace("{toxinidir}", self.tox_path))
        return rv

    def replace_path(self, requirements_path):
        lines = []
        with open(requirements_path, "rb") as f:
            self.file_cache[requirements_path] = f.read()
            f.seek(0)
            for line in f:
                m = requirements_re.match(line)
                if not m:
                    lines.append(line)
                else:
                    key = m.group(1)
                    path = local_requirements[key]
                    lines.append(
                        b"-e %s\n"
                        % (os.path.join(self.top_src_path, path).encode("utf8"),)
                    )

        with open(requirements_path, "wb") as f:
            for line in lines:
                f.write(line)

        with open(requirements_path, "rb") as f:
            print(f.read())


def get_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--no-tools",
        dest="tools",
        action="store_false",
        default=True,
        help="Don't run the tools unittests",
    )
    parser.add_argument(
        "--no-wptrunner",
        dest="wptrunner",
        action="store_false",
        default=True,
        help="Don't run the wptrunner unittests",
    )
    parser.add_argument(
        "tox_kwargs", nargs=argparse.REMAINDER, help="Arguments to pass through to tox"
    )
    return parser


def run(top_src_dir, tools=True, wptrunner=True, tox_kwargs=None, **kwargs):
    tox_paths = []
    if tox_kwargs is None:
        tox_kwargs = []
    if tools:
        tox_paths.append(
            os.path.join(top_src_dir, "testing", "web-platform", "tests", "tools")
        )
    if wptrunner:
        tox_paths.append(
            os.path.join(
                top_src_dir, "testing", "web-platform", "tests", "tools", "wptrunner"
            )
        )

    success = True

    for tox_path in tox_paths:
        with ReplaceRequirements(top_src_dir, tox_path):
            cmd = ["tox"] + tox_kwargs
            try:
                subprocess.check_call(cmd, cwd=tox_path)
            except subprocess.CalledProcessError:
                success = False
    return success


def main():
    kwargs = vars(get_parser().parse_args())
    top_src_path = os.path.abspath(os.path.join(here, os.pardir, os.pardir))
    return run(top_src_path, **kwargs)


if __name__ == "__main__":
    if not main():
        sys.exit(1)
