# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy
import contextlib
import importlib
from pathlib import Path
import tempfile
import shutil

from mozperftest.test import pick_test
from mozperftest.system import pick_system
from mozperftest.metrics import pick_metrics
from mozperftest.layers import Layers
from mozperftest.utils import download_file


SYSTEM, TEST, METRICS = 0, 1, 2


class MachEnvironment:
    def __init__(self, mach_cmd, flavor="script", **kwargs):
        self._mach_cmd = mach_cmd
        self._mach_args = dict(
            [(self._normalize(key), value) for key, value in kwargs.items()]
        )
        self.layers = []
        # XXX do something with flavors, etc
        if flavor != "script":
            raise NotImplementedError(flavor)
        for layer in (pick_system, pick_test, pick_metrics):
            self.add_layer(layer(self, flavor, mach_cmd))
        self.tmp_dir = tempfile.mkdtemp()
        self._load_hooks()

    @contextlib.contextmanager
    def frozen(self):
        self.freeze()
        try:
            # used to trigger __enter__/__exit__
            with self:
                yield self
        finally:
            self.unfreeze()

    def _normalize(self, name):
        if name.startswith("--"):
            name = name[2:]
        return name.replace("-", "_")

    def set_arg(self, name, value):
        """Sets the argument"""
        # see if we want to restrict to existing keys
        self._mach_args[self._normalize(name)] = value

    def get_arg(self, name, default=None, layer=None):
        name = self._normalize(name)
        marker = object()
        res = self._mach_args.get(name, marker)
        if res is marker:
            # trying with the name prefixed with the layer name
            if layer is not None and not name.startswith(layer.name):
                name = "%s_%s" % (layer.name, name)
                return self._mach_args.get(name, default)
            return default
        return res

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
        has_exc_handler = self.has_hook("on_exception")

        # run the system and test layers
        with self.layers[SYSTEM] as syslayer, self.layers[TEST] as testlayer:
            metadata = syslayer(metadata)
            try:
                metadata = testlayer(metadata)
            except Exception as e:
                if has_exc_handler:
                    # if the hook returns True, we abort and return
                    # without error. If it returns False, we continue
                    # the loop. The hook can also raise an exception or
                    # re-raise this exception.
                    if not self.run_hook("on_exception", testlayer, e):
                        return metadata
                else:
                    raise

        # then run the metrics layers
        with self.layers[METRICS] as metrics:
            metadata = metrics(metadata)

        return metadata

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        return

    def cleanup(self):
        if self.tmp_dir is None:
            return
        shutil.rmtree(self.tmp_dir)
        self.tmp_dir = None

    def _load_hooks(self):
        self._hooks = None
        hooks = self.get_arg("hooks")
        if hooks is None:
            return

        if hooks.startswith("http"):
            target = Path(self.tmp_dir, hooks.split("/")[-1])
            hooks = download_file(hooks, target)
        else:
            hooks = Path(hooks)

        if hooks.exists():
            spec = importlib.util.spec_from_file_location("hooks", str(hooks))
            hooks = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(hooks)
            self._hooks = hooks
        else:
            raise IOError(str(hooks))

    def has_hook(self, name):
        return hasattr(self._hooks, name)

    def run_hook(self, name, *args, **kw):
        if self._hooks is None:
            return
        if not hasattr(self._hooks, name):
            return
        return getattr(self._hooks, name)(self, *args, **kw)
