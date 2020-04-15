# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import signal
import re

from buildconfig import substs
from mozboot.util import get_state_dir
from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozprocess import ProcessHandler

CLANG_FORMAT_NOT_FOUND = """
Could not find clang-format! Install clang-format with:

    $ ./mach bootstrap

And make sure that it is in the PATH
""".strip()


def parse_issues(config, output, paths, log):

    diff_line = re.compile("^(.*):(.*):(.*): warning: .*;(.*);(.*)")
    results = []
    for line in output:
        match = diff_line.match(line)
        file, line_no, col, diff, diff2 = match.groups()
        log.debug("file={} line={} col={} diff={} diff2={}".format(
            file, line_no, col, diff, diff2))
        d = diff + "\n" + diff2
        res = {
            "path": file,
            "diff": d,
            "level": "warning",
            "lineno": line_no,
            "column": col,
        }
        results.append(result.from_config(config, **res))

    return results


class ClangFormatProcess(ProcessHandler):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs["stream"] = False
        kwargs["universal_newlines"] = True
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(config, cmd):
    proc = ClangFormatProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return proc.output


def get_clang_format_binary():
    """
    Returns the path of the first clang-format binary available
    if not found returns None
    """
    binary = os.environ.get("CLANG_FORMAT")
    if binary:
        return binary

    clang_tools_path = os.path.join(get_state_dir(), "clang-tools")
    bin_path = os.path.join(clang_tools_path, "clang-tidy", "bin")
    return os.path.join(bin_path, "clang-format" + substs.get('BIN_SUFFIX', ''))


def is_ignored_path(ignored_dir_re, topsrcdir, f):
    # Remove up to topsrcdir in pathname and match
    if f.startswith(topsrcdir + '/'):
        match_f = f[len(topsrcdir + '/'):]
    else:
        match_f = f
    return re.match(ignored_dir_re, match_f)


def remove_ignored_path(paths, topsrcdir, log):
    path_to_third_party = os.path.join(topsrcdir, '.clang-format-ignore')

    ignored_dir = []
    with open(path_to_third_party, 'r') as fh:
        for line in fh:
            # In case it starts with a space
            line = line.strip()
            # Remove comments and empty lines
            if line.startswith('#') or len(line) == 0:
                continue
            # The regexp is to make sure we are managing relative paths
            ignored_dir.append(r"^[\./]*" + line.rstrip())

    # Generates the list of regexp
    ignored_dir_re = '(%s)' % '|'.join(ignored_dir)

    path_list = []
    for f in paths:
        if is_ignored_path(ignored_dir_re, topsrcdir, f):
            # Early exit if we have provided an ignored directory
            log.debug("Ignored third party code '{0}'".format(f))
            continue
        path_list.append(f)

    return path_list


def lint(paths, config, fix=None, **lintargs):
    log = lintargs['log']
    paths = list(expand_exclusions(paths, config, lintargs['root']))

    # We ignored some specific files for a bunch of reasons.
    # Not using excluding to avoid duplication
    if lintargs.get('use_filters', True):
        paths = remove_ignored_path(paths, lintargs['root'], log)

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
    if fix:
        cmd_args.append("-i")
    else:
        cmd_args.append("--dry-run")
    base_command = cmd_args + paths
    log.debug("Command: {}".format(' '.join(cmd_args)))
    output = run_process(config, base_command)
    output_list = []

    if len(output) % 3 != 0:
        raise Exception("clang-format output should be a multiple of 3. Output: %s" % output)

    for i in range(0, len(output), 3):
        # Merge the element 3 by 3 (clang-format output)
        line = output[i]
        line += ";" + output[i+1]
        line += ";" + output[i+2]
        output_list.append(line)

    if fix:
        # clang-format is able to fix all issues so don't bother parsing the output.
        return []
    return parse_issues(config, output_list, paths, log)
