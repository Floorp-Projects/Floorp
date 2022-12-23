# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import subprocess
import sys


def parse_outputs(crate_output, dep_outputs, pass_l_flag):
    env = {}
    args = []

    def parse_line(line):
        if line.startswith("cargo:"):
            return line[len("cargo:") :].split("=", 1)

    def parse_file(f):
        with open(f) as fh:
            return [parse_line(line.rstrip()) for line in fh.readlines()]

    for f in dep_outputs:
        for entry in parse_file(f):
            if not entry:
                continue
            key, value = entry
            if key == "rustc-link-search":
                args += ["-L", value]
            elif key == "rustc-flags":
                flags = value.split()
                for flag, val in zip(flags[0::2], flags[1::2]):
                    if flag == "-l" and f == crate_output:
                        args += ["-l", val]
                    elif flag == "-L":
                        args += ["-L", val]
                    else:
                        raise Exception(
                            "Unknown flag passed through "
                            '"cargo:rustc-flags": "%s"' % flag
                        )
            elif key == "rustc-link-lib" and f == crate_output:
                args += ["-l", value]
            elif key == "rustc-cfg" and f == crate_output:
                args += ["--cfg", value]
            elif key == "rustc-env" and f == crate_output:
                env_key, env_value = value.split("=", 1)
                env[env_key] = env_value
            elif key == "rerun-if-changed":
                pass
            elif key == "rerun-if-env-changed":
                pass
            elif key == "warning":
                pass
            elif key:
                # Todo: Distinguish between direct and transitive
                # dependencies so we can pass metadata environment
                # variables correctly.
                pass

    return env, args


def wrap_rustc(args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--crate-out", nargs="?")
    parser.add_argument("--deps-out", nargs="*")
    parser.add_argument("--cwd")
    parser.add_argument("--pass-l-flag", action="store_true")
    parser.add_argument("--cmd", nargs=argparse.REMAINDER)
    args = parser.parse_args(args)

    new_env, new_args = parse_outputs(args.crate_out, args.deps_out, args.pass_l_flag)
    os.environ.update(new_env)
    return subprocess.Popen(args.cmd + new_args, cwd=args.cwd).wait()


if __name__ == "__main__":
    sys.exit(wrap_rustc(sys.argv[1:]))
