# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from tempfile import TemporaryDirectory

import mozpack.path as mozpath
from buildconfig import topsrcdir
from mozunit import MockedOpen, main

from common import BaseConfigureTest
from mozbuild.util import ReadOnlyNamespace


class IndexSearch:
    def should_replace_task(self, task, *args):
        return f'fake-task-id-for-{task["index"][0]}'


class TestBootstrap(BaseConfigureTest):
    @staticmethod
    def import_module(module):
        if module == "taskgraph.optimize.strategies":
            return ReadOnlyNamespace(IndexSearch=IndexSearch)

    # This method asserts the expected result of bootstrapping for the given
    # argument (`arg`) to configure.
    # - `states` is a 3-tuple of the initial state of each of the 3 fake toolchains
    # bootstrapped by the test. Valid states are:
    #   - `True`: the toolchain was already installed and up-to-date.
    #   - `"old"`: an old version of the toolchain was already installed.
    #   - `False`: the toolchain was not already installed.
    # - `bootstrapped` is a 3-tuple representing whether each toolchain is expected
    # to have been actively bootstrapped by configure.
    # - `in_path` is a 3-tuple representing whether the path for each toolchain is
    # expected to have been added to the `bootstrap_search_path`. Valid values are:
    #   - `True`: the toolchain path was prepended to `bootstrap_search_path`.
    #   - `False`: the toolchain path is not in `bootstrap_search_path`.
    def assertBootstrap(self, arg, states, bootstrapped, in_path):
        called_for = []

        def mock_mach(input, args, cwd):
            assert os.path.basename(args[0]) == "mach"
            assert args[1] == "--log-no-times"
            assert args[2] == "artifact"
            assert args[3] == "toolchain"
            assert args[4] == "--from-task"
            toolchain_dir = os.path.basename(args[-1].rpartition(".artifact")[0])
            try:
                os.mkdir(os.path.join(cwd, toolchain_dir))
            except FileExistsError:
                pass
            called_for.append(toolchain_dir)
            return "", "", 0

        sandbox = self.get_sandbox(
            {
                sys.executable: mock_mach,
            },
            {},
            [arg] if arg else [],
            {},
        )
        dep = sandbox._depends[sandbox["vcs_checkout_type"]]
        getattr(sandbox, "__value_for_depends")[(dep,)] = None
        dep = sandbox._depends[sandbox["original_path"]]
        getattr(sandbox, "__value_for_depends")[(dep,)] = ["dummy"]

        tmp_dir = TemporaryDirectory()
        dep = sandbox._depends[sandbox["toolchains_base_dir"]]
        getattr(sandbox, "__value_for_depends")[(dep,)] = tmp_dir.name

        dep = sandbox._depends[sandbox["bootstrap_toolchain_tasks"]]
        getattr(sandbox, "__value_for_depends")[(dep,)] = ReadOnlyNamespace(
            prefix="linux64",
            tasks={
                "toolchain-foo": {
                    "index": ["fake.index.foo"],
                    "artifact": "public/foo.artifact",
                    "extract": True,
                },
                "toolchain-linux64-bar": {
                    "index": ["fake.index.bar"],
                    "artifact": "public/bar.artifact",
                    "extract": True,
                },
                "toolchain-linux64-qux": {
                    "index": ["fake.index.qux"],
                    "artifact": "public/qux.artifact",
                    "extract": True,
                },
            },
        )
        toolchains = ["foo", "bar", "qux"]
        for t in toolchains:
            exec(f'{t} = bootstrap_search_path("{t}")', sandbox)
        sandbox._wrapped_importlib = ReadOnlyNamespace(import_module=self.import_module)
        for t, in_path, b, state in zip(toolchains, in_path, bootstrapped, states):
            if in_path == "append":
                expected = ["dummy", mozpath.join(tmp_dir.name, t)]
            elif in_path:
                expected = [mozpath.join(tmp_dir.name, t), "dummy"]
            else:
                expected = ["dummy"]
            if state:
                os.mkdir(os.path.join(tmp_dir.name, t))
                indices = os.path.join(tmp_dir.name, "indices")
                os.makedirs(indices, exist_ok=True)
                with open(os.path.join(indices, t), "w") as fh:
                    fh.write("old" if state == "old" else t)
            self.assertEqual(sandbox._value_for(sandbox[t]), expected, msg=t)
            self.assertEqual(t in called_for, bool(b), msg=t)

    def test_bootstrap(self):
        milestone_path = os.path.join(topsrcdir, "config", "milestone.txt")

        with MockedOpen({milestone_path: "124.0a1"}):
            self.assertBootstrap(
                "--disable-bootstrap",
                (True, "old", False),
                (False, False, False),
                (False, False, False),
            )
            self.assertBootstrap(
                None,
                (True, "old", False),
                (False, True, True),
                (True, True, True),
            )

        with MockedOpen({milestone_path: "124.0"}):
            self.assertBootstrap(
                "--disable-bootstrap",
                (True, "old", False),
                (False, False, False),
                (False, False, False),
            )
            self.assertBootstrap(
                None,
                (True, "old", False),
                (False, False, False),
                (True, True, False),
            )

        for milestone in ("124.0a1", "124.0"):
            with MockedOpen({milestone_path: milestone}):
                # With `--enable-bootstrap`, anything is bootstrappable
                self.assertBootstrap(
                    "--enable-bootstrap",
                    (True, "old", False),
                    (False, True, True),
                    (True, True, True),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=no-update",
                    (True, "old", False),
                    (False, False, True),
                    (True, True, True),
                )

                # With `--enable-bootstrap=foo,bar`, only foo and bar are bootstrappable
                self.assertBootstrap(
                    "--enable-bootstrap=foo,bar",
                    (False, "old", False),
                    (True, True, False),
                    (True, True, False),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=foo",
                    (True, "old", True),
                    (False, False, False),
                    (True, True, True),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=no-update,foo,bar",
                    (False, "old", False),
                    (True, False, False),
                    (True, True, False),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=no-update,foo",
                    (True, "old", True),
                    (False, False, False),
                    (True, True, True),
                )

                # With `--enable-bootstrap=-foo`, anything is bootstrappable, except foo
                self.assertBootstrap(
                    "--enable-bootstrap=-foo",
                    (True, False, "old"),
                    (False, True, True),
                    (True, True, True),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=-foo",
                    (False, False, "old"),
                    (False, True, True),
                    (False, True, True),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=no-update,-foo",
                    (True, False, "old"),
                    (False, True, False),
                    (True, True, True),
                )
                self.assertBootstrap(
                    "--enable-bootstrap=no-update,-foo",
                    (False, False, "old"),
                    (False, True, False),
                    (False, True, True),
                )

                # Corner case.
                self.assertBootstrap(
                    "--enable-bootstrap=-foo,foo,bar",
                    (False, False, "old"),
                    (False, True, False),
                    (False, True, True),
                )


if __name__ == "__main__":
    main()
