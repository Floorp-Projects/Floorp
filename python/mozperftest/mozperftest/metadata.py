# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import importlib
import copy
import contextlib


class Metadata:
    def __init__(self, mach_cmd, flavor, **kwargs):
        self._mach_cmd = mach_cmd
        self._mach_args = kwargs
        self.flavor = flavor
        self.browser = {"prefs": {}}
        self._load_hooks()
        self._saved_mach_args = None
        self._result = None
        self._output = None

    def set_output(self, output):
        self._output = output

    def get_output(self):
        return self._output

    def set_result(self, result):
        self._result = result

    def get_result(self):
        return self._result

    def set_arg(self, name, value):
        self._mach_args[name] = value

    def get_arg(self, name, default=None):
        return self._mach_args.get(name, default)

    def update_browser_prefs(self, prefs):
        self.browser["prefs"].update(prefs)

    def get_browser_prefs(self):
        return self.browser["prefs"]

    @contextlib.contextmanager
    def frozen(self):
        self._freeze()
        try:
            yield self
        finally:
            self._unfreeze()

    def _freeze(self):
        # freeze args  (XXX do we need to freeze more?)
        self._saved_mach_args = copy.deepcopy(self._mach_args)

    def _unfreeze(self):
        self._mach_args = self._saved_mach_args
        self._saved_mach_args = None

    def _load_hooks(self):
        self._hooks = None
        if "hooks" in self._mach_args:
            hooks = self._mach_args["hooks"]
            if os.path.exists(hooks):
                spec = importlib.util.spec_from_file_location("hooks", hooks)
                hooks = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(hooks)
                self._hooks = hooks

    def run_hook(self, name, **kw):
        if self._hooks is None:
            return
        if not hasattr(self._hooks, name):
            return
        getattr(self._hooks, name)(self, **kw)
