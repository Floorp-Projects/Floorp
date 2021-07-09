# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import importlib
import tempfile
from pathlib import Path
import shutil

from mozperftest.utils import download_file, MachLogger


_LOADED_MODULES = {}


class Hooks(MachLogger):
    def __init__(self, mach_cmd, hook_module=None):
        MachLogger.__init__(self, mach_cmd)
        self.tmp_dir = tempfile.mkdtemp()

        if hook_module is None:
            self._hooks = None
            return

        if not isinstance(hook_module, Path):
            if hook_module.startswith("http"):
                target = Path(self.tmp_dir, hook_module.split("/")[-1])
                hook_module = download_file(hook_module, target)
            else:
                hook_module = Path(hook_module)

        if hook_module.exists():
            path = str(hook_module)
            if path not in _LOADED_MODULES:
                spec = importlib.util.spec_from_file_location("hooks", path)
                hook_module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(hook_module)
                _LOADED_MODULES[path] = hook_module
            self._hooks = _LOADED_MODULES[path]
        else:
            raise IOError("Could not find hook module. %s" % str(hook_module))

    def cleanup(self):
        if self.tmp_dir is None:
            return
        shutil.rmtree(self.tmp_dir)
        self.tmp_dir = None

    def exists(self, name):
        if self._hooks is None:
            return False
        return hasattr(self._hooks, name)

    def get(self, name):
        if self._hooks is None:
            return False
        return getattr(self._hooks, name)

    def run(self, name, *args, **kw):
        if self._hooks is None:
            return
        if not hasattr(self._hooks, name):
            return
        self.debug("Running hook %s" % name)
        return getattr(self._hooks, name)(*args, **kw)
