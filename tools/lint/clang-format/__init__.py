# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import signal
import subprocess
import sys
import xml.etree.ElementTree as ET

from mozboot.util import get_tools_dir
from mozlint import result
from mozlint.pathutils import expand_exclusions

CLANG_FORMAT_NOT_FOUND = """
Could not find clang-format! It should've been installed automatically - \
please report a bug here:
https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox%20Build%20System&component=Lint%20and%20Formatting
""".strip()


def setup(root, mach_command_context, **lintargs):
    if get_clang_format_binary():
        return 0

    from mozbuild.code_analysis.mach_commands import get_clang_tools

    rc, _ = get_clang_tools(mach_command_context)
    if rc:
        return 1


def run_process(config, cmd):
    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    signal.signal(signal.SIGINT, orig)
    try:
        output, _ = proc.communicate()
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return output


def get_clang_format_binary():
    """
    Returns the path of the first clang-format binary available
    if not found returns None
    """
    binary = os.environ.get("CLANG_FORMAT")
    if binary:
        return binary

    clang_tools_path = os.path.join(get_tools_dir(), "clang-tools")
    bin_path = os.path.join(clang_tools_path, "clang-tidy", "bin")
    binary = os.path.join(bin_path, "clang-format")

    if sys.platform.startswith("win"):
        binary += ".exe"

    if not os.path.isfile(binary):
        return None

    return binary


def is_ignored_path(ignored_dir_re, topsrcdir, f):
    # Remove up to topsrcdir in pathname and match
    if f.startswith(topsrcdir + "/"):
        match_f = f[len(topsrcdir + "/") :]
    else:
        match_f = f
    return re.match(ignored_dir_re, match_f)


def remove_ignored_path(paths, topsrcdir, log):
    path_to_third_party = os.path.join(topsrcdir, ".clang-format-ignore")

    ignored_dir = []
    with open(path_to_third_party, "r") as fh:
        for l in fh:
            # In case it starts with a space
            line = l.strip()
            # Remove comments and empty lines
            if line.startswith("#") or len(line) == 0:
                continue
            # The regexp is to make sure we are managing relative paths
            ignored_dir.append(r"^[\./]*" + line.rstrip())

    # Generates the list of regexp
    ignored_dir_re = "(%s)" % "|".join(ignored_dir)

    path_list = []
    for f in paths:
        if is_ignored_path(ignored_dir_re, topsrcdir, f):
            # Early exit if we have provided an ignored directory
            log.debug("Ignored third party code '{0}'".format(f))
            continue
        path_list.append(f)

    return path_list


def lint(paths, config, fix=None, **lintargs):
    log = lintargs["log"]
    paths = list(expand_exclusions(paths, config, lintargs["root"]))

    # We ignored some specific files for a bunch of reasons.
    # Not using excluding to avoid duplication
    if lintargs.get("use_filters", True):
        paths = remove_ignored_path(paths, lintargs["root"], log)

    # An empty path array can occur when the user passes in `-n`. If we don't
    # return early in this case, rustfmt will attempt to read stdin and hang.
    if not paths:
        return []

    binary = get_clang_format_binary()

    if not binary:
        print(CLANG_FORMAT_NOT_FOUND)
        if "MOZ_AUTOMATION" in os.environ:
            return 1
        return []

    cmd_args = [binary]

    base_command = cmd_args + ["--version"]
    version = run_process(config, base_command).rstrip("\r\n")
    log.debug("Version: {}".format(version))

    cmd_args.append("--output-replacements-xml")
    base_command = cmd_args + paths
    log.debug("Command: {}".format(" ".join(cmd_args)))
    output = run_process(config, base_command)

    def replacement(parser):
        for end, e in parser.read_events():
            assert end == "end"
            if e.tag == "replacement":
                item = {k: int(v) for k, v in e.items()}
                assert sorted(item.keys()) == ["length", "offset"]
                item["with"] = (e.text or "").encode("utf-8")
                yield item

    # When given multiple paths as input, --output-replacements-xml
    # will output one xml per path, in the order they are given, but
    # XML parsers don't know how to handle that, so do it manually.
    parser = None
    replacements = []
    for l in output.split("\n"):
        line = l.rstrip("\r\n")
        if line.startswith("<?xml "):
            if parser:
                replacements.append(list(replacement(parser)))
            parser = ET.XMLPullParser(["end"])
        parser.feed(line)
    replacements.append(list(replacement(parser)))

    results = []
    fixed = 0
    for path, replacement in zip(paths, replacements):
        if not replacement:
            continue
        with open(path, "rb") as fh:
            data = fh.read()

        linenos = []
        patched_data = b""
        last_offset = 0
        lineno_before = 1
        lineno_after = 1

        for item in replacement:
            offset = item["offset"]
            length = item["length"]
            replace_with = item["with"]
            since_last_offset = data[last_offset:offset]
            replaced = data[offset : offset + length]

            lines_since_last_offset = since_last_offset.count(b"\n")
            lineno_before += lines_since_last_offset
            lineno_after += lines_since_last_offset
            start_lineno = (lineno_before, lineno_after)

            lineno_before += replaced.count(b"\n")
            lineno_after += replace_with.count(b"\n")
            end_lineno = (lineno_before, lineno_after)

            if linenos and start_lineno[0] <= linenos[-1][1][0]:
                linenos[-1] = (linenos[-1][0], end_lineno)
            else:
                linenos.append((start_lineno, end_lineno))

            patched_data += since_last_offset + replace_with
            last_offset = offset + len(replaced)
        patched_data += data[last_offset:]

        lines_before = data.decode("utf-8", "replace").splitlines()
        lines_after = patched_data.decode("utf-8", "replace").splitlines()
        for (start_before, start_after), (end_before, end_after) in linenos:
            diff = "".join(
                "-" + l + "\n" for l in lines_before[start_before - 1 : end_before]
            )
            diff += "".join(
                "+" + l + "\n" for l in lines_after[start_after - 1 : end_after]
            )

            results.append(
                result.from_config(
                    config,
                    path=path,
                    diff=diff,
                    level="warning",
                    lineno=start_before,
                    column=0,
                )
            )

        if fix:
            with open(path, "wb") as fh:
                fh.write(patched_data)
            fixed += len(linenos)

    return {"results": results, "fixed": fixed}
