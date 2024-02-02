# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import pathlib
import re
import signal
import tempfile
import threading

from mozdevice import ADBDevice
from mozlog import get_proxy_logger
from mozprocess import ProcessHandler

from mozperftest.layers import Layer
from mozperftest.utils import (
    ON_TRY,
    download_file,
    get_output_dir,
    get_pretty_app_name,
    install_package,
)

LOG = get_proxy_logger(component="proxy")
HERE = os.path.dirname(__file__)


class OutputHandler(object):
    def __init__(self):
        self.proc = None
        self.port = None
        self.port_event = threading.Event()

    def __call__(self, line):
        line = line.strip()
        if not line:
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
                self.port = int(m.group(1))
                self.port_event.set()
            LOG.log_raw(data)
        else:
            self.process_output(json.dumps(data))

    def finished(self):
        self.port_event.set()

    def process_output(self, line):
        if self.proc is None:
            LOG.process_output(line)
        else:
            LOG.process_output(self.proc.pid, line)

    def wait_for_port(self):
        self.port_event.wait()
        return self.port


class ProxyRunner(Layer):
    """Use a proxy"""

    name = "proxy"
    activated = False

    arguments = {
        "mode": {
            "type": str,
            "choices": ["record", "playback"],
            "help": "Proxy server mode. Use `playback` to replay from the provided file(s). "
            "Use `record` to generate a new recording at the path specified by `--file`. "
            "playback - replay from provided file. "
            "record - generate a new recording at the specified path.",
        },
        "file": {
            "type": str,
            "nargs": "+",
            "help": "The playback files to replay, or the file that a recording will be saved to. "
            "For playback, it can be any combination of the following: zip file, manifest file, "
            "or a URL to zip/manifest file. "
            "For recording, it's a zip fle.",
        },
        "perftest-page": {
            "type": str,
            "default": None,
            "help": "This option can be used to specify a single test to record rather than "
            "having to continuously modify the pageload_sites.json. This flag should only be "
            "used by the perftest team and selects items from "
            "`testing/performance/pageload_sites.json` based on the name field. Note that "
            "the login fields won't be checked with a request such as this (i.e. it overrides "
            "those settings).",
        },
        "deterministic": {
            "action": "store_true",
            "default": False,
            "help": "If set, the deterministic JS script will be injected into the pages.",
        },
    }

    def __init__(self, env, mach_cmd):
        super(ProxyRunner, self).__init__(env, mach_cmd)
        self.proxy = None
        self.tmpdir = None

    def setup(self):
        try:
            import mozproxy  # noqa: F401
        except ImportError:
            # Install mozproxy and its vendored deps.
            mozbase = pathlib.Path(self.mach_cmd.topsrcdir, "testing", "mozbase")
            mozproxy_deps = ["mozinfo", "mozlog", "mozproxy"]
            for i in mozproxy_deps:
                install_package(
                    self.mach_cmd.virtualenv_manager, pathlib.Path(mozbase, i)
                )

        # set MOZ_HOST_BIN to find cerutil. Required to set certifcates on android
        os.environ["MOZ_HOST_BIN"] = self.mach_cmd.bindir

    def run(self, metadata):
        self.metadata = metadata
        replay_file = self.get_arg("file")

        # Check if we have a replay file
        if replay_file is None:
            raise ValueError("Proxy file not provided!!")

        if replay_file is not None and replay_file.startswith("http"):
            self.tmpdir = tempfile.TemporaryDirectory()
            target = pathlib.Path(self.tmpdir.name, "recording.zip")
            self.info("Downloading %s" % replay_file)
            download_file(replay_file, target)
            replay_file = target

        self.info("Setting up the proxy")

        command = [
            self.mach_cmd.virtualenv_manager.python_path,
            "-m",
            "mozproxy.driver",
            "--topsrcdir=" + self.mach_cmd.topsrcdir,
            "--objdir=" + self.mach_cmd.topobjdir,
            "--profiledir=" + self.get_arg("profile-directory"),
        ]

        if not ON_TRY:
            command.extend(["--local"])

        if metadata.flavor == "mobile-browser":
            command.extend(["--tool=%s" % "mitmproxy-android"])
            command.extend(["--binary=android"])
            command.extend(
                [f"--app={get_pretty_app_name(self.get_arg('android-app-name'))}"]
            )
        else:
            command.extend(["--tool=%s" % "mitmproxy"])
            # XXX See bug 1712337, we need a single point where we can get the binary used from
            # this is required to make it work localy
            binary = self.get_arg("browsertime-binary")
            if binary is None:
                binary = self.mach_cmd.get_binary_path()
            command.extend(["--binary=%s" % binary])

        if self.get_arg("mode") == "record":
            output = self.get_arg("output")
            if output is None:
                output = pathlib.Path(self.mach_cmd.topsrcdir, "artifacts")
            results_dir = get_output_dir(output)

            command.extend(["--mode", "record"])
            command.append(str(pathlib.Path(results_dir, replay_file)))
        elif self.get_arg("mode") == "playback":
            command.extend(["--mode", "playback"])
            command.append(str(replay_file))
        else:
            raise ValueError("Proxy mode not provided please provide proxy mode")

        inject_deterministic = self.get_arg("deterministic")
        if inject_deterministic:
            command.extend(["--deterministic"])

        print(" ".join(command))
        self.output_handler = OutputHandler()
        self.proxy = ProcessHandler(
            command,
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
            "network.proxy.http": "127.0.0.1",
            "network.proxy.http_port": port,
            "network.proxy.ssl": "127.0.0.1",
            "network.proxy.ssl_port": port,
            "network.proxy.no_proxies_on": "127.0.0.1",
        }
        browser_prefs = metadata.get_options("browser_prefs")
        browser_prefs.update(prefs)

        if metadata.flavor == "mobile-browser":
            self.info("Setting reverse port fw for android device")
            device = ADBDevice()
            device.create_socket_connection("reverse", "tcp:%s" % port, "tcp:%s" % port)

        return metadata

    def teardown(self):
        err = None
        if self.proxy is not None:
            returncode = self.proxy.wait(0)
            if returncode is not None:
                err = ValueError(
                    "mozproxy terminated early with return code %d" % returncode
                )
            else:
                kill_signal = getattr(signal, "CTRL_BREAK_EVENT", signal.SIGINT)
                os.kill(self.proxy.pid, kill_signal)
                self.proxy.wait()
            self.proxy = None
        if self.tmpdir is not None:
            self.tmpdir.cleanup()
            self.tmpdir = None

        if err:
            raise err
