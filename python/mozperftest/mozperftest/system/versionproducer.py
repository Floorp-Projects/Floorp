# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozperftest.layers import Layer
from mozperftest.system.binarysetup import get_app_setup


class VersionProducer(Layer):
    """Determines the version of a given application."""

    name = "version-producer"
    activated = True
    arguments = {}

    def __init__(self, env, mach_cmd):
        super(VersionProducer, self).__init__(env, mach_cmd)
        self.env = env
        self.mach_cmd = mach_cmd

    def setup(self):
        pass

    def teardown(self):
        pass

    def run(self, metadata):
        binary = metadata.binary
        if not binary:
            return metadata

        app = self.get_arg("app")
        app_setup = get_app_setup(app)
        if app_setup is None:
            self.warning(f"Unknown app {app} for binary setup.")
            return metadata

        setup_obj = app_setup(app, self)
        metadata.binary_version = setup_obj.get_binary_version(
            binary, apk_path=self.get_arg("android-install-apk")
        )
        return metadata
