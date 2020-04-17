# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import mozinfo
from mozproxy import get_playback
from mozperftest.layers import Layer


HERE = os.path.dirname(__file__)


class ProxyRunner(Layer):
    name = "proxy"

    arguments = {
        "--proxy": {"action": "store_true", "default": False, "help": "Use a proxy"}
    }

    def __init__(self, env, mach_cmd):
        super(ProxyRunner, self).__init__(env, mach_cmd)
        self.proxy = None

    def setup(self):
        pass

    def __call__(self, metadata):
        self.metadata = metadata
        if not self.get_arg("proxy"):
            return metadata

        # replace with artifacts
        config = {
            "run_local": True,
            "playback_tool": "mitmproxy",
            "host": "localhost",
            "binary": self.mach_cmd.get_binary_path(),
            "obj_path": self.mach_cmd.topobjdir,
            "platform": mozinfo.os,
            "playback_files": [os.path.join(HERE, "example.dump")],
            "app": "firefox",
        }
        self.info("setting up the proxy")
        self.proxy = get_playback(config)
        if self.proxy is not None:
            self.proxy.start()
            port = str(self.proxy.port)
            prefs = {}
            prefs["network.proxy.type"] = 1
            prefs["network.proxy.http"] = "localhost"
            prefs["network.proxy.http_port"] = port
            prefs["network.proxy.ssl"] = "localhost"
            prefs["network.proxy.ssl_port"] = port
            prefs["network.proxy.no_proxies_on"] = "localhost"
            metadata.update_browser_prefs(prefs)
        return metadata

    def teardown(self):
        if self.proxy is not None:
            self.proxy.stop()
            self.proxy = None
