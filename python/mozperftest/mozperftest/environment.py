# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy
import contextlib
import importlib
import os

from mozperftest.browser import pick_browser
from mozperftest.system import pick_system
from mozperftest.metrics import pick_metrics
from mozperftest.layers import Layers


SYSTEM, BROWSER, METRICS = 0, 1, 2


class MachEnvironment:
    def __init__(self, mach_cmd, flavor="script", **kwargs):
        self._mach_cmd = mach_cmd
        self._mach_args = kwargs
        self.layers = []
        # XXX do something with flavors, etc
        if flavor != "script":
            raise NotImplementedError(flavor)
        for layer in (pick_system, pick_browser, pick_metrics):
            self.add_layer(layer(self, flavor, mach_cmd))
        self._load_hooks()

    @contextlib.contextmanager
    def frozen(self):
        self.freeze()
        try:
            yield self
        finally:
            self.unfreeze()

    def set_arg(self, name, value):
        """Sets the argument"""
        # see if we want to restrict to existing keys
        self._mach_args[name] = value

    def get_arg(self, name, default=None):
        return self._mach_args.get(name, default)

    def get_layer(self, name):
        for layer in self.layers:
            if isinstance(layer, Layers):
                found = layer.get_layer(name)
                if found is not None:
                    return found
            elif layer.name == name:
                return layer
        return None

    def add_layer(self, layer):
        self.layers.append(layer)

    def freeze(self):
        # freeze args  (XXX do we need to freeze more?)
        self._saved_mach_args = copy.deepcopy(self._mach_args)

    def unfreeze(self):
        self._mach_args = self._saved_mach_args
        self._saved_mach_args = None

    def run(self, metadata):
        for layer in self.layers:
            metadata = layer(metadata)
        return metadata

    def __enter__(self):
        for layer in self.layers:
            layer.__enter__()
        return self

    def __exit__(self, type, value, traceback):
        for layer in self.layers:
            layer.__exit__(type, value, traceback)

    def _load_hooks(self):
        self._hooks = None
        hooks = self.get_arg("hooks")
        if hooks is not None and os.path.exists(hooks):
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
