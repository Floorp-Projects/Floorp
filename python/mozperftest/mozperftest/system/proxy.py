# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import re
import signal
import threading

from mozlog import get_proxy_logger
from mozperftest.layers import Layer
from mozperftest.utils import install_package
from mozprocess import ProcessHandler


LOG = get_proxy_logger(component="proxy")
HERE = os.path.dirname(__file__)


class OutputHandler(object):
    def __init__(self):
        self.proc = None
        self.port = None
        self.port_event = threading.Event()

    def __call__(self, line):
        if not line.strip():
            return
        line = line.decode("utf-8", errors="replace")

        try:
            data = json.loads(line)
        except ValueError:
            self.process_output(line)
            return

        if isinstance(data, dict) and "action" in data:
            # Retrieve the port number for the proxy server from the logs of
            # our subprocess.
            m = re.match(r"Proxy running on port (\d+)", data.get("message", ""))
            if m:
                self.port = m.group(1)
                self.port_event.set()
            LOG.log_raw(data)
        else:
            self.process_output(json.dumps(data))

    def finished(self):
        self.port_event.set()

    def process_output(self, line):
        LOG.process_output(self.proc.pid, line)

    def wait_for_port(self):
        self.port_event.wait()
        return self.port


class ProxyRunner(Layer):
    """Use a proxy
    """

    name = "proxy"
    activated = False

    def __init__(self, env, mach_cmd):
        super(ProxyRunner, self).__init__(env, mach_cmd)
        self.proxy = None

    def setup(self):
        # Install mozproxy and its vendored deps.
        mozbase = os.path.join(self.mach_cmd.topsrcdir, "testing", "mozbase")
        mozproxy_deps = ["mozinfo", "mozlog", "mozproxy"]
        for i in mozproxy_deps:
            install_package(self.mach_cmd.virtualenv_manager, os.path.join(mozbase, i))

    def run(self, metadata):
        self.metadata = metadata

        self.info("Setting up the proxy")
        # replace with artifacts
        self.output_handler = OutputHandler()
        self.proxy = ProcessHandler(
            [
                "mozproxy",
                "--local",
                "--binary=" + self.mach_cmd.get_binary_path(),
                "--topsrcdir=" + self.mach_cmd.topsrcdir,
                "--objdir=" + self.mach_cmd.topobjdir,
                os.path.join(HERE, "example.dump"),
            ],
            processOutputLine=self.output_handler,
            onFinish=self.output_handler.finished,
        )
        self.output_handler.proc = self.proxy
        self.proxy.run()

        # Wait until we've retrieved the proxy server's port number so we can
        # configure the browser properly.
        port = self.output_handler.wait_for_port()
        if port is None:
            raise ValueError("Unable to retrieve the port number from mozproxy")
        self.info("Received port number %s from mozproxy" % port)

        prefs = {
            "network.proxy.type": 1,
            "network.proxy.http": "localhost",
            "network.proxy.http_port": port,
            "network.proxy.ssl": "localhost",
            "network.proxy.ssl_port": port,
            "network.proxy.no_proxies_on": "localhost",
        }
        browser_prefs = metadata.get_options("browser_prefs")
        browser_prefs.update(prefs)
        return metadata

    def teardown(self):
        if self.proxy is not None:
            kill_signal = getattr(signal, "CTRL_BREAK_EVENT", signal.SIGINT)
            os.kill(self.proxy.pid, kill_signal)
            self.proxy.wait()
            self.proxy = None
