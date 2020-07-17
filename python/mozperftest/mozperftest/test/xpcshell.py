# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pathlib import Path
from collections import defaultdict

from mozperftest.layers import Layer
from mozperftest.utils import temp_dir, silence


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
    """Runs an xpcshell test.
    """

    name = "xpcshell"
    activated = True
    user_exception = True

    arguments = {
        "cycles": {"type": int, "default": 13, "help": "Number of full cycles"},
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

    def setup(self):
        self.mach_cmd._activate_virtualenv()

    def run(self, metadata):
        tests = self.get_arg("tests", [])
        if len(tests) != 1:
            # for now we support one single test
            raise NotImplementedError(str(tests))

        test = Path(tests[0])
        if not test.exists():
            raise FileNotFoundError(str(test))

        # let's grab the manifest
        manifest = Path(test.parent, "xpcshell.ini")
        if not manifest.exists():
            raise FileNotFoundError(str(manifest))

        import runxpcshelltests

        xpcshell = runxpcshelltests.XPCShellTests(log=self)
        kwargs = {}
        kwargs["testPaths"] = test.name
        kwargs["verbose"] = True
        kwargs["xpcshell"] = self.mach_cmd.get_binary_path("xpcshell")
        kwargs["mozInfo"] = str(Path(self.topobjdir, "mozinfo.json"))
        kwargs["symbolsPath"] = str(Path(self.distdir, "crashreporter-symbols"))
        kwargs["logfiles"] = True
        kwargs["profileName"] = "firefox"
        kwargs["pluginsPath"] = str(Path(self.distdir, "plugins"))
        kwargs["testingModulesDir"] = str(Path(self.topobjdir, "_tests/modules"))
        kwargs["utility_path"] = self.bindir
        kwargs["manifest"] = manifest
        kwargs["totalChunks"] = 1
        cycles = self.get_arg("cycles", 1)
        self.info("Running %d cycles" % cycles)

        for cycle in range(cycles):
            with temp_dir() as tmp, silence():
                kwargs["tempDir"] = tmp
                if not xpcshell.runTests(kwargs):
                    raise XPCShellTestError()
            self.info("Cycle %d" % (cycle + 1))
        self.info("tests done.")

        results = defaultdict(list)
        for m in self.metrics:
            for key, val in m.items():
                results[key].append(val)

        metadata.add_result(
            {
                "name": test.name,
                "framework": {"name": "xpcshell"},
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
