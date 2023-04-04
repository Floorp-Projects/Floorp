# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import socketserver
import threading
import time
from pathlib import Path

from mozlog import get_proxy_logger

from mozperftest.layers import Layer
from mozperftest.utils import install_package

LOG = get_proxy_logger(component="proxy")
HERE = os.path.dirname(__file__)


class PingServer(Layer):
    """Runs the edgeping layer"""

    name = "pingserver"
    activated = False

    arguments = {}

    def setup(self):
        # Install edgeping and requests
        deps = ["edgeping==0.1", "requests==2.9.1"]
        for dep in deps:
            install_package(self.mach_cmd.virtualenv_manager, dep)

    def _wait_for_server(self, endpoint):
        import requests

        start = time.monotonic()
        while True:
            try:
                requests.get(endpoint, timeout=0.1)
                return
            except Exception:
                # we want to wait at most 5sec.
                if time.monotonic() - start > 5.0:
                    raise
                time.sleep(0.01)

    def run(self, metadata):
        from edgeping.server import PingHandling

        self.verbose = self.get_arg("verbose")
        self.metadata = metadata
        self.debug("Starting the Edgeping server")
        self.httpd = socketserver.TCPServer(("localhost", 0), PingHandling)
        self.server_thread = threading.Thread(target=self.httpd.serve_forever)
        # the chosen socket gets picked in the constructor so we can grab it here
        address = self.httpd.server_address
        self.endpoint = f"http://{address[0]}:{address[1]}"
        self.server_thread.start()
        self._wait_for_server(self.endpoint + "/status")

        self.debug(f"Edgeping coserver running at {self.endpoint}")
        prefs = {
            "toolkit.telemetry.server": self.endpoint,
            "telemetry.fog.test.localhost_port": address[1],
            "datareporting.healthreport.uploadEnabled": True,
            "datareporting.policy.dataSubmissionEnabled": True,
            "toolkit.telemetry.enabled": True,
            "toolkit.telemetry.unified": True,
            "toolkit.telemetry.shutdownPingSender.enabled": True,
            "datareporting.policy.dataSubmissionPolicyBypassNotification": True,
            "toolkit.telemetry.send.overrideOfficialCheck": True,
        }
        if self.verbose:
            prefs["toolkit.telemetry.log.level"] = "Trace"
            prefs["toolkit.telemetry.log.dump"] = True

        browser_prefs = metadata.get_options("browser_prefs")
        browser_prefs.update(prefs)
        return metadata

    def teardown(self):
        import requests

        self.info("Grabbing the pings")
        pings = requests.get(f"{self.endpoint}/pings").json()
        output = Path(self.get_arg("output"), "telemetry.json")
        self.info(f"Writing in {output}")
        with output.open("w") as f:
            f.write(json.dumps(pings))

        self.debug("Stopping the Edgeping coserver")
        self.httpd.shutdown()
        self.server_thread.join()
