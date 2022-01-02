# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import traceback
from mozperftest.utils import MachLogger


class StopRunError(Exception):
    pass


class Layer(MachLogger):
    # layer name
    name = "unset"

    # activated by default ?
    activated = False

    # list of arguments grabbed by PerftestArgumentParser
    arguments = {}

    # If true, calls on_exception() on errors
    user_exception = False

    def __init__(self, env, mach_command):
        MachLogger.__init__(self, mach_command)
        self.return_code = 0
        self.mach_cmd = mach_command
        self.run_process = mach_command.run_process
        self.env = env

    def _normalize_arg(self, name):
        if name.startswith("--"):
            name = name[2:]
        if not name.startswith(self.name):
            name = "%s-%s" % (self.name, name)
        return name.replace("-", "_")

    def get_arg_names(self):
        return [self._normalize_arg(arg) for arg in self.arguments]

    def set_arg(self, name, value):
        """Sets the argument"""
        name = self._normalize_arg(name)
        if name not in self.get_arg_names():
            raise KeyError(
                "%r tried to set %r, but does not own it" % (self.name, name)
            )
        return self.env.set_arg(name, value)

    def get_arg(self, name, default=None):
        return self.env.get_arg(name, default, self)

    def __enter__(self):
        self.debug("Running %s:setup" % self.name)
        self.setup()
        return self

    def __exit__(self, type, value, traceback):
        # XXX deal with errors here
        self.debug("Running %s:teardown" % self.name)
        self.teardown()

    def __call__(self, metadata):
        has_exc_handler = self.env.hooks.exists("on_exception")
        self.debug("Running %s:run" % self.name)
        try:
            metadata = self.run(metadata)
        except Exception as e:
            if self.user_exception and has_exc_handler:
                self.error("User handled error")
                for line in traceback.format_exc().splitlines():
                    self.error(line)
                resume_run = self.env.hooks.run("on_exception", self.env, self, e)
                if resume_run:
                    return metadata
                raise StopRunError()
            else:
                raise
        return metadata

    def setup(self):
        pass

    def teardown(self):
        pass

    def run(self, metadata):
        return metadata


class Layers(Layer):
    def __init__(self, env, mach_command, factories):
        super(Layers, self).__init__(env, mach_command)

        def _active(layer):
            # if it's activated by default, see if we need to deactivate
            # it by looking for the --no-layername option
            if layer.activated:
                return not env.get_arg("no-" + layer.name, False)
            # if it's deactivated by default, we look for --layername
            return env.get_arg(layer.name, False)

        self.layers = [
            factory(env, mach_command) for factory in factories if _active(factory)
        ]
        self.env = env
        self._counter = -1

    def _normalize_arg(self, name):
        if name.startswith("--"):
            name = name[2:]
        return name.replace("-", "_")

    def get_layer(self, name):
        for layer in self.layers:
            if layer.name == name:
                return layer
        return None

    @property
    def name(self):
        return " + ".join([l.name for l in self.layers])

    def __iter__(self):
        self._counter = -1
        return self

    def __next__(self):
        self._counter += 1
        try:
            return self.layers[self._counter]
        except IndexError:
            raise StopIteration

    def __enter__(self):
        self.setup()
        return self

    def __exit__(self, type, value, traceback):
        # XXX deal with errors here
        self.teardown()

    def setup(self):
        for layer in self.layers:
            self.debug("Running %s:setup" % layer.name)
            layer.setup()

    def teardown(self):
        for layer in self.layers:
            self.debug("Running %s:teardown" % layer.name)
            layer.teardown()

    def __call__(self, metadata):
        for layer in self.layers:
            metadata = layer(metadata)
        return metadata

    def set_arg(self, name, value):
        """Sets the argument"""
        name = self._normalize_arg(name)
        found = False
        for layer in self.layers:
            if name in layer.get_arg_names():
                found = True
                break

        if not found:
            raise KeyError(
                "%r tried to set %r, but does not own it" % (self.name, name)
            )

        return self.env.set_arg(name, value)

    def get_arg(self, name, default=None):
        return self.env.get_arg(name, default)
