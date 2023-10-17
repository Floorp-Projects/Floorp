# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
import types
from pathlib import Path
from unittest.mock import patch

from mozunit import main

from mach.base import MachError
from mach.test.conftest import TestBase


class Entry:
    """Stub replacement for pkg_resources.EntryPoint"""

    def __init__(self, providers):
        self.providers = providers

    def load(self):
        def _providers():
            return self.providers

        return _providers


class TestEntryPoints(TestBase):
    """Test integrating with setuptools entry points"""

    provider_dir = Path(__file__).parent.resolve() / "providers"

    def _run_help(self):
        return self._run_mach(["help"], entry_point="mach.providers")

    @patch("pkg_resources.iter_entry_points")
    def test_load_entry_point_from_directory(self, mock):
        # Ensure parent module is present otherwise we'll (likely) get
        # an error due to unknown parent.
        if "mach.commands" not in sys.modules:
            mod = types.ModuleType("mach.commands")
            sys.modules["mach.commands"] = mod

        mock.return_value = [Entry([self.provider_dir])]
        # Mach error raised due to conditions_invalid.py
        with self.assertRaises(MachError):
            self._run_help()

    @patch("pkg_resources.iter_entry_points")
    def test_load_entry_point_from_file(self, mock):
        mock.return_value = [Entry([self.provider_dir / "basic.py"])]

        result, stdout, stderr = self._run_help()
        self.assertIsNone(result)
        self.assertIn("cmd_foo", stdout)


if __name__ == "__main__":
    main()
