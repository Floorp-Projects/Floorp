# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess
import sys

import buildconfig
import six

from mozbuild.shellutil import quote as shell_quote

SCRIPT_ALLOWLIST = [buildconfig.topsrcdir + "/devtools/client/shared/build/build.js"]

ALLOWLIST_ERROR = """
%s is not
in SCRIPT_ALLOWLIST in python/mozbuild/mozbuild/action/node.py.
Using NodeJS from moz.build is currently in beta, and node
scripts to be executed need to be added to the allowlist and
reviewed by a build peer so that we can get a better sense of
how support should evolve. (To consult a build peer, raise a
question in the #build channel at https://chat.mozilla.org.)
"""


def is_script_in_allowlist(script_path):
    if script_path in SCRIPT_ALLOWLIST:
        return True

    return False


def execute_node_cmd(node_cmd_list):
    """Execute the given node command list.

    Arguments:
    node_cmd_list -- a list of the command and arguments to be executed

    Returns:
    The set of dependencies which should trigger this command to be re-run.
    This is ultimately returned to the build system for use by the backend
    to ensure that incremental rebuilds happen when any dependency changes.

    The node script is expected to output lines for all of the dependencies
    to stdout, each prefixed by the string "dep:".  These lines will make up
    the returned set of dependencies.  Any line not so-prefixed will simply be
    printed to stderr instead.
    """

    try:
        print('Executing "{}"'.format(shell_quote(*node_cmd_list)), file=sys.stderr)
        sys.stderr.flush()

        # We need to redirect stderr to a pipe because
        # https://github.com/nodejs/node/issues/14752 causes issues with make.
        proc = subprocess.Popen(
            node_cmd_list, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        stdout, stderr = proc.communicate()
        retcode = proc.wait()

        if retcode != 0:
            print(stderr, file=sys.stderr)
            sys.stderr.flush()
            sys.exit(retcode)

        # Process the node script output
        #
        # XXX Starting with an empty list means that node scripts can
        # (intentionally or inadvertently) remove deps.  Do we want this?
        deps = []
        for line in stdout.splitlines():
            line = six.ensure_text(line)
            if "dep:" in line:
                deps.append(line.replace("dep:", ""))
            else:
                print(line, file=sys.stderr)
                sys.stderr.flush()

        return set(deps)

    except subprocess.CalledProcessError as err:
        # XXX On Mac (and elsewhere?) "OSError: [Errno 13] Permission denied"
        # (at least sometimes) means "node executable not found".  Can we
        # disambiguate this from real "Permission denied" errors so that we
        # can log such problems more clearly?
        print(
            """Failed with %s.  Be sure to check that your mozconfig doesn't
            have --disable-nodejs in it.  If it does, try removing that line and
            building again."""
            % str(err),
            file=sys.stderr,
        )
        sys.exit(1)


def generate(output, node_script, *files):
    """Call the given node_script to transform the given modules.

    Arguments:
    output -- a dummy file, used by the build system.  Can be ignored.
    node_script -- the script to be executed.  Must be in the SCRIPT_ALLOWLIST
    files -- files to be transformed, will be passed to the script as arguments

    Returns:
    The set of dependencies which should trigger this command to be re-run.
    This is ultimately returned to the build system for use by the backend
    to ensure that incremental rebuilds happen when any dependency changes.
    """

    node_interpreter = buildconfig.substs.get("NODEJS")
    if not node_interpreter:
        print(
            """NODEJS not set.  Be sure to check that your mozconfig doesn't
            have --disable-nodejs in it.  If it does, try removing that line
            and building again.""",
            file=sys.stderr,
        )
        sys.exit(1)

    node_script = six.ensure_text(node_script)
    if not isinstance(node_script, six.text_type):
        print(
            "moz.build file didn't pass a valid node script name to execute",
            file=sys.stderr,
        )
        sys.exit(1)

    if not is_script_in_allowlist(node_script):
        print(ALLOWLIST_ERROR % (node_script), file=sys.stderr)
        sys.exit(1)

    node_cmd_list = [node_interpreter, node_script]
    node_cmd_list.extend(files)

    return execute_node_cmd(node_cmd_list)
