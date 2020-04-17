# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys

import mozpack.path as mozpath

from mozperftest.layers import Layer
from mozperftest.utils import silence


class NodeRunner(Layer):
    name = "node"

    def __init__(self, env, mach_cmd):
        super(NodeRunner, self).__init__(env, mach_cmd)
        self.topsrcdir = mach_cmd.topsrcdir
        self._mach_context = mach_cmd._mach_context
        self.python_path = mach_cmd.virtualenv_manager.python_path

        from mozbuild.nodeutil import find_node_executable

        self.node_path = os.path.abspath(find_node_executable()[0])

    def setup(self):
        """Install the Node.js package.
        """
        self.mach_cmd._activate_virtualenv()
        self.verify_node_install()

    def node(self, args):
        """Invoke node (interactively) with the given arguments."""
        return self.run_process(
            [self.node_path] + args,
            append_env=self.append_env(),
            pass_thru=True,  # Allow user to run Node interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir),
        )

    def append_env(self, append_path=True):
        # Ensure that bare `node` and `npm` in scripts, including post-install
        # scripts, finds the binary we're invoking with.  Without this, it's
        # easy for compiled extensions to get mismatched versions of the Node.js
        # extension API.
        path = os.environ.get("PATH", "").split(os.pathsep) if append_path else []
        node_dir = os.path.dirname(self.node_path)
        path = [node_dir] + path

        return {
            "PATH": os.pathsep.join(path),
            # Bug 1560193: The JS library browsertime uses to execute commands
            # (execa) will muck up the PATH variable and put the directory that
            # node is in first in path. If this is globally-installed node,
            # that means `/usr/bin` will be inserted first which means that we
            # will get `/usr/bin/python` for `python`.
            #
            # Our fork of browsertime supports a `PYTHON` environment variable
            # that points to the exact python executable to use.
            "PYTHON": self.python_path,
        }

    def verify_node_install(self):
        # check if Node is installed
        sys.path.append(mozpath.join(self.topsrcdir, "tools", "lint", "eslint"))
        import setup_helper

        with silence():
            node_valid = setup_helper.check_node_executables_valid()
        if not node_valid:
            # running again to get details printed out
            setup_helper.check_node_executables_valid()
            raise ValueError("Can't find Node. did you run ./mach bootstrap ?")

        return True
