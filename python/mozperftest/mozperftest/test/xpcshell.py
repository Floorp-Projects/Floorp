# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from collections import defaultdict
from distutils.dir_util import copy_tree
from pathlib import Path

from mozperftest.layers import Layer
from mozperftest.utils import NoPerfMetricsError, temp_dir


class XPCShellTestError(Exception):
    pass


class XPCShellData:
    def open_data(self, data):
        return {
            "name": "xpcshell",
            "subtest": data["name"],
            "data": [
                {"file": "xpcshell", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
        }

    def transform(self, data):
        return data

    merge = transform


class XPCShell(Layer):
    """Runs an xpcshell test."""

    name = "xpcshell"
    activated = True

    arguments = {
        "cycles": {"type": int, "default": 13, "help": "Number of full cycles"},
        "binary": {
            "type": str,
            "default": None,
            "help": (
                "xpcshell binary path. If not provided, "
                "looks for it in the source tree."
            ),
        },
        "mozinfo": {
            "type": str,
            "default": None,
            "help": (
                "mozinfo binary path. If not provided, looks for it in the obj tree."
            ),
        },
        "xre-path": {"type": str, "default": None, "help": "XRE path."},
        "nodejs": {"type": str, "default": None, "help": "nodejs binary path."},
    }

    def __init__(self, env, mach_cmd):
        super(XPCShell, self).__init__(env, mach_cmd)
        self.topsrcdir = mach_cmd.topsrcdir
        self._mach_context = mach_cmd._mach_context
        self.python_path = mach_cmd.virtualenv_manager.python_path
        self.topobjdir = mach_cmd.topobjdir
        self.distdir = mach_cmd.distdir
        self.bindir = mach_cmd.bindir
        self.statedir = mach_cmd.statedir
        self.metrics = []
        self.topsrcdir = mach_cmd.topsrcdir

    def setup(self):
        pass

    def run(self, metadata):
        test = Path(metadata.script["filename"])

        # let's grab the manifest
        manifest = Path(test.parent, "xpcshell.ini")
        if not manifest.exists():
            manifest = Path(test.parent, "xpcshell.toml")
            if not manifest.exists():
                raise FileNotFoundError(str(manifest))

        nodejs = self.get_arg("nodejs")
        if nodejs is not None:
            os.environ["MOZ_NODE_PATH"] = nodejs

        import runxpcshelltests

        verbose = self.get_arg("verbose")
        xpcshell = runxpcshelltests.XPCShellTests(log=self)
        kwargs = {}
        kwargs["testPaths"] = test.name
        kwargs["verbose"] = verbose
        binary = self.get_arg("xpcshell_binary")
        if binary is None:
            binary = self.mach_cmd.get_binary_path("xpcshell")
        kwargs["xpcshell"] = binary
        binary = Path(binary)
        mozinfo = self.get_arg("mozinfo")
        if mozinfo is None:
            mozinfo = binary.parent / ".." / "mozinfo.json"
            if not mozinfo.exists():
                mozinfo = Path(self.topobjdir, "mozinfo.json")
        else:
            mozinfo = Path(mozinfo)

        kwargs["mozInfo"] = str(mozinfo)
        kwargs["symbolsPath"] = str(Path(self.distdir, "crashreporter-symbols"))
        kwargs["logfiles"] = True
        kwargs["profileName"] = "firefox"
        plugins = binary.parent / "plugins"
        if not plugins.exists():
            plugins = Path(self.distdir, "plugins")
        kwargs["pluginsPath"] = str(plugins)

        modules = Path(self.topobjdir, "_tests", "modules")
        if not modules.exists():
            modules = binary.parent / "modules"

        kwargs["testingModulesDir"] = str(modules)
        kwargs["utility_path"] = self.bindir
        kwargs["manifest"] = str(manifest)
        kwargs["totalChunks"] = 1
        xre_path = self.get_arg("xre-path")
        if xre_path is not None:
            self.info(f"Copying {xre_path} elements to {binary.parent}")
            copy_tree(xre_path, str(binary.parent), update=True)

        http3server = binary.parent / "http3server"
        if http3server.exists():
            kwargs["http3server"] = str(http3server)

        cycles = self.get_arg("cycles", 1)
        self.info("Running %d cycles" % cycles)

        for cycle in range(cycles):
            self.info("Cycle %d" % (cycle + 1))
            with temp_dir() as tmp:
                kwargs["tempDir"] = tmp
                if not xpcshell.runTests(kwargs):
                    raise XPCShellTestError()

        self.info("tests done.")

        results = defaultdict(list)
        for m in self.metrics:
            for key, val in m.items():
                results[key].append(val)

        if len(results.items()) == 0:
            raise NoPerfMetricsError("xpcshell")

        metadata.add_result(
            {
                "name": test.name,
                "framework": {"name": "mozperftest"},
                "transformer": "mozperftest.test.xpcshell:XPCShellData",
                "results": [
                    {"values": measures, "name": subtest}
                    for subtest, measures in results.items()
                ],
            }
        )

        return metadata

    def log_raw(self, data, **kw):
        if data["action"] != "log":
            return
        if data["message"].strip('"') != "perfMetrics":
            self.info(data["message"])
            return
        self.metrics.append(data["extra"])

    def process_output(self, procid, line, command):
        self.info(line)

    def dummy(self, *args, **kw):
        pass

    test_end = suite_start = suite_end = test_start = dummy
