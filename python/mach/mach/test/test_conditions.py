# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path

from buildconfig import topsrcdir
from mozunit import main

from mach.base import MachError
from mach.command_util import load_commands_from_file
from mach.main import Mach
from mach.registrar import Registrar
from mach.test.conftest import PROVIDER_DIR, TestBase


def _make_populate_context(include_extra_attributes):
    def _populate_context(key=None):
        if key is None:
            return

        attributes = {
            "topdir": topsrcdir,
        }
        if include_extra_attributes:
            attributes["foo"] = True
            attributes["bar"] = False

        try:
            return attributes[key]
        except KeyError:
            raise AttributeError(key)

    return _populate_context


_populate_bare_context = _make_populate_context(False)
_populate_context = _make_populate_context(True)


class TestConditions(TestBase):
    """Tests for conditionally filtering commands."""

    def _run(self, args, context_handler=_populate_bare_context):
        return self._run_mach(
            args, Path("conditions.py"), context_handler=context_handler
        )

    def test_conditions_pass(self):
        """Test that a command which passes its conditions is runnable."""

        self.assertEqual((0, "", ""), self._run(["cmd_condition_true"]))
        self.assertEqual((0, "", ""), self._run(["cmd_foo_ctx"], _populate_context))

    def test_invalid_context_message(self):
        """Test that commands which do not pass all their conditions
        print the proper failure message."""

        def is_bar():
            """Bar must be true"""

        fail_conditions = [is_bar]

        for name in ("cmd_condition_false", "cmd_condition_true_and_false"):
            result, stdout, stderr = self._run([name])
            self.assertEqual(1, result)

            fail_msg = Registrar._condition_failed_message(name, fail_conditions)
            self.assertEqual(fail_msg.rstrip(), stdout.rstrip())

        for name in ("cmd_bar_ctx", "cmd_foobar_ctx"):
            result, stdout, stderr = self._run([name], _populate_context)
            self.assertEqual(1, result)

            fail_msg = Registrar._condition_failed_message(name, fail_conditions)
            self.assertEqual(fail_msg.rstrip(), stdout.rstrip())

    def test_invalid_type(self):
        """Test that a condition which is not callable raises an exception."""

        m = Mach(str(Path.cwd()))
        m.define_category("testing", "Mach unittest", "Testing for mach core", 10)
        self.assertRaises(
            MachError,
            load_commands_from_file,
            PROVIDER_DIR / "conditions_invalid.py",
        )

    def test_help_message(self):
        """Test that commands that are not runnable do not show up in help."""

        result, stdout, stderr = self._run(["help"], _populate_context)
        self.assertIn("cmd_condition_true", stdout)
        self.assertNotIn("cmd_condition_false", stdout)
        self.assertNotIn("cmd_condition_true_and_false", stdout)
        self.assertIn("cmd_foo_ctx", stdout)
        self.assertNotIn("cmd_bar_ctx", stdout)
        self.assertNotIn("cmd_foobar_ctx", stdout)


if __name__ == "__main__":
    main()
