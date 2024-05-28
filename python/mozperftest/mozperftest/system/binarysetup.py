# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.layers import Layer
from mozperftest.system.setups import get_app_setup


class BinarySetup(Layer):
    """Determines the version of a given application."""

    name = "binary-setup"
    activated = True
    arguments = {}

    def __init__(self, env, mach_cmd):
        super(BinarySetup, self).__init__(env, mach_cmd)
        self.env = env
        self.mach_cmd = mach_cmd
        self.get_binary_path = mach_cmd.get_binary_path

    def setup(self):
        pass

    def teardown(self):
        pass

    def run(self, metadata):
        app = self.get_arg("app")
        app_setup = get_app_setup(app)
        if app_setup is None:
            self.warning(f"Unknown app {app} for binary setup.")
            return metadata

        setup_obj = app_setup(app, self)
        binary = self.get_arg("binary")
        if binary is None:
            binary = setup_obj.setup_binary()
        metadata.binary = binary

        return metadata
