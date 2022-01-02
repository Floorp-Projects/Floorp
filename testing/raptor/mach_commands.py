# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Originally taken from /talos/mach_commands.py

# Integrates raptor mozharness with mach

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import os
import shutil
import socket
import subprocess
import sys

import mozfile
from mach.decorators import Command
from mach.util import get_state_dir
from mozbuild.base import (
    MozbuildObject,
    BinaryNotFoundException,
)
from mozbuild.base import MachCommandConditions as Conditions

HERE = os.path.dirname(os.path.realpath(__file__))

BENCHMARK_REPOSITORY = "https://github.com/mozilla/perf-automation"
BENCHMARK_REVISION = "54c3c3d9d3f651f0d8ebb809d25232f72b5b06f2"

ANDROID_BROWSERS = ["geckoview", "refbrow", "fenix", "chrome-m"]


class RaptorRunner(MozbuildObject):
    def run_test(self, raptor_args, kwargs):
        """Setup and run mozharness.

        We want to do a few things before running Raptor:

        1. Clone mozharness
        2. Make the config for Raptor mozharness
        3. Run mozharness
        """
        self.init_variables(raptor_args, kwargs)
        self.setup_benchmarks()
        self.make_config()
        self.write_config()
        self.make_args()
        return self.run_mozharness()

    def init_variables(self, raptor_args, kwargs):
        self.raptor_args = raptor_args

        if kwargs.get("host") == "HOST_IP":
            kwargs["host"] = os.environ["HOST_IP"]
        self.host = kwargs["host"]
        self.is_release_build = kwargs["is_release_build"]
        self.memory_test = kwargs["memory_test"]
        self.power_test = kwargs["power_test"]
        self.cpu_test = kwargs["cpu_test"]
        self.live_sites = kwargs["live_sites"]
        self.disable_perf_tuning = kwargs["disable_perf_tuning"]
        self.conditioned_profile = kwargs["conditioned_profile"]
        self.device_name = kwargs["device_name"]
        self.enable_marionette_trace = kwargs["enable_marionette_trace"]
        self.browsertime_visualmetrics = kwargs["browsertime_visualmetrics"]

        if Conditions.is_android(self) or kwargs["app"] in ANDROID_BROWSERS:
            self.binary_path = None
        else:
            self.binary_path = kwargs.get("binary") or self.get_binary_path()

        self.python = sys.executable

        self.raptor_dir = os.path.join(self.topsrcdir, "testing", "raptor")
        self.mozharness_dir = os.path.join(self.topsrcdir, "testing", "mozharness")
        self.config_file_path = os.path.join(
            self._topobjdir, "testing", "raptor-in_tree_conf.json"
        )

        self.virtualenv_script = os.path.join(
            self.topsrcdir, "third_party", "python", "virtualenv", "virtualenv.py"
        )
        self.virtualenv_path = os.path.join(self._topobjdir, "testing", "raptor-venv")

    def setup_benchmarks(self):
        """Make sure benchmarks are linked to the proper location in the objdir.

        Benchmarks can either live in-tree or in an external repository. In the latter
        case also clone/update the repository if necessary.
        """
        external_repo_path = os.path.join(get_state_dir(), "performance-tests")

        print("Updating external benchmarks from {}".format(BENCHMARK_REPOSITORY))

        try:
            subprocess.check_output(["git", "--version"])
        except Exception as ex:
            print(
                "Git is not available! Please install git and "
                "ensure it is included in the terminal path"
            )
            raise ex

        if not os.path.isdir(external_repo_path):
            print("Cloning the benchmarks to {}".format(external_repo_path))
            subprocess.check_call(
                ["git", "clone", BENCHMARK_REPOSITORY, external_repo_path]
            )
        else:
            subprocess.check_call(["git", "checkout", "master"], cwd=external_repo_path)
            subprocess.check_call(["git", "pull"], cwd=external_repo_path)

        subprocess.check_call(
            ["git", "checkout", BENCHMARK_REVISION], cwd=external_repo_path
        )

        # Link or copy benchmarks to the objdir
        benchmark_paths = (
            os.path.join(external_repo_path, "benchmarks"),
            os.path.join(self.topsrcdir, "third_party", "webkit", "PerformanceTests"),
        )

        benchmark_dest = os.path.join(self.topobjdir, "testing", "raptor", "benchmarks")
        if not os.path.isdir(benchmark_dest):
            os.makedirs(benchmark_dest)

        for benchmark_path in benchmark_paths:
            for name in os.listdir(benchmark_path):
                path = os.path.join(benchmark_path, name)
                dest = os.path.join(benchmark_dest, name)
                if not os.path.isdir(path) or name.startswith("."):
                    continue

                if hasattr(os, "symlink") and os.name != "nt":
                    if not os.path.exists(dest):
                        os.symlink(path, dest)
                else:
                    # Clobber the benchmark in case a recent update removed any files.
                    mozfile.remove(dest)
                    shutil.copytree(path, dest)

    def make_config(self):
        default_actions = [
            "populate-webroot",
            "create-virtualenv",
            "install-chromium-distribution",
            "run-tests",
        ]
        self.config = {
            "run_local": True,
            "binary_path": self.binary_path,
            "repo_path": self.topsrcdir,
            "raptor_path": self.raptor_dir,
            "obj_path": self.topobjdir,
            "log_name": "raptor",
            "virtualenv_path": self.virtualenv_path,
            "pypi_url": "http://pypi.org/simple",
            "base_work_dir": self.mozharness_dir,
            "exes": {
                "python": self.python,
                "virtualenv": [self.python, self.virtualenv_script],
            },
            "title": socket.gethostname(),
            "default_actions": default_actions,
            "raptor_cmd_line_args": self.raptor_args,
            "host": self.host,
            "power_test": self.power_test,
            "memory_test": self.memory_test,
            "cpu_test": self.cpu_test,
            "live_sites": self.live_sites,
            "disable_perf_tuning": self.disable_perf_tuning,
            "conditioned_profile": self.conditioned_profile,
            "is_release_build": self.is_release_build,
            "device_name": self.device_name,
            "enable_marionette_trace": self.enable_marionette_trace,
            "browsertime_visualmetrics": self.browsertime_visualmetrics,
        }

        sys.path.insert(0, os.path.join(self.topsrcdir, "tools", "browsertime"))
        try:
            import mach_commands as browsertime

            # We don't set `browsertime_{chromedriver,geckodriver} -- those will be found by
            # browsertime in its `node_modules` directory, which is appropriate for local builds.
            # We don't set `browsertime_ffmpeg` yet: it will need to be on the path.  There is code
            # to configure the environment including the path in
            # `tools/browsertime/mach_commands.py` but integrating it here will take more effort.
            self.config.update(
                {
                    "browsertime_node": browsertime.node_path(),
                    "browsertime_browsertimejs": browsertime.browsertime_path(),
                    "browsertime_vismet_script": browsertime.visualmetrics_path(),
                }
            )

            def _get_browsertime_package():
                with open(
                    os.path.join(
                        self.topsrcdir,
                        "tools",
                        "browsertime",
                        "node_modules",
                        "browsertime",
                        "package.json",
                    )
                ) as package:
                    return json.load(package)

            def _get_browsertime_resolved():
                try:
                    with open(
                        os.path.join(
                            self.topsrcdir,
                            "tools",
                            "browsertime",
                            "node_modules",
                            ".package-lock.json",
                        )
                    ) as package_lock:
                        return json.load(package_lock)["packages"][
                            "node_modules/browsertime"
                        ]["resolved"]
                except FileNotFoundError:
                    # Older versions of node/npm add this metadata to package.json
                    return _get_browsertime_package()["_from"]

            def _should_install():
                # If browsertime doesn't exist, install it
                if not os.path.exists(
                    self.config["browsertime_browsertimejs"]
                ) or not os.path.exists(self.config["browsertime_vismet_script"]):
                    return True

                # Browsertime exists, check if it's outdated
                with open(
                    os.path.join(self.topsrcdir, "tools", "browsertime", "package.json")
                ) as new:
                    new_pkg = json.load(new)

                return not _get_browsertime_resolved().endswith(
                    new_pkg["devDependencies"]["browsertime"]
                )

            def _get_browsertime_version():
                # Returns the (version number, current commit) used
                return (
                    _get_browsertime_package()["version"],
                    _get_browsertime_resolved(),
                )

            # Check if browsertime scripts exist and try to install them if
            # they aren't
            if _should_install():
                # TODO: Make this "integration" nicer in the near future
                print("Missing browsertime files...attempting to install")
                subprocess.check_call(
                    [
                        os.path.join(self.topsrcdir, "mach"),
                        "browsertime",
                        "--setup",
                        "--clobber",
                    ]
                )
                if _should_install():
                    raise Exception(
                        "Failed installation attempt. Cannot find browsertime scripts. "
                        "Run `./mach browsertime --setup --clobber` to set it up."
                    )

            print("Using browsertime version %s from %s" % _get_browsertime_version())

        finally:
            sys.path = sys.path[1:]

    def make_args(self):
        self.args = {
            "config": {},
            "initial_config_file": self.config_file_path,
        }

    def write_config(self):
        try:
            config_file = open(self.config_file_path, "w")
            config_file.write(json.dumps(self.config))
            config_file.close()
        except IOError as e:
            err_str = "Error writing to Raptor Mozharness config file {0}:{1}"
            print(err_str.format(self.config_file_path, str(e)))
            raise e

    def run_mozharness(self):
        sys.path.insert(0, self.mozharness_dir)
        from mozharness.mozilla.testing.raptor import Raptor

        raptor_mh = Raptor(
            config=self.args["config"],
            initial_config_file=self.args["initial_config_file"],
        )
        return raptor_mh.run()


def create_parser():
    sys.path.insert(0, HERE)  # allow to import the raptor package
    from raptor.cmdline import create_parser

    return create_parser(mach_interface=True)


@Command(
    "raptor",
    category="testing",
    description="Run Raptor performance tests.",
    parser=create_parser,
)
def run_raptor(command_context, **kwargs):
    # Defers this import so that a transitive dependency doesn't
    # stop |mach bootstrap| from running
    from raptor.power import enable_charging, disable_charging

    build_obj = command_context

    is_android = Conditions.is_android(build_obj) or kwargs["app"] in ANDROID_BROWSERS

    if is_android:
        from mozrunner.devices.android_device import (
            verify_android_device,
            InstallIntent,
        )
        from mozdevice import ADBDeviceFactory

        install = (
            InstallIntent.NO if kwargs.pop("noinstall", False) else InstallIntent.YES
        )
        verbose = False
        if (
            kwargs.get("log_mach_verbose")
            or kwargs.get("log_tbpl_level") == "debug"
            or kwargs.get("log_mach_level") == "debug"
            or kwargs.get("log_raw_level") == "debug"
        ):
            verbose = True
        if not verify_android_device(
            build_obj,
            install=install,
            app=kwargs["binary"],
            verbose=verbose,
            xre=True,
        ):  # Equivalent to 'run_local' = True.
            return 1

    # Remove mach global arguments from sys.argv to prevent them
    # from being consumed by raptor. Treat any item in sys.argv
    # occuring before "raptor" as a mach global argument.
    argv = []
    in_mach = True
    for arg in sys.argv:
        if not in_mach:
            argv.append(arg)
        if arg.startswith("raptor"):
            in_mach = False

    raptor = command_context._spawn(RaptorRunner)
    device = None

    try:
        if kwargs["power_test"] and is_android:
            device = ADBDeviceFactory(verbose=True)
            disable_charging(device)
        return raptor.run_test(argv, kwargs)
    except BinaryNotFoundException as e:
        command_context.log(
            logging.ERROR, "raptor", {"error": str(e)}, "ERROR: {error}"
        )
        command_context.log(logging.INFO, "raptor", {"help": e.help()}, "{help}")
        return 1
    except Exception as e:
        print(repr(e))
        return 1
    finally:
        if kwargs["power_test"] and device:
            enable_charging(device)


@Command(
    "raptor-test",
    category="testing",
    description="Run Raptor performance tests.",
    parser=create_parser,
)
def run_raptor_test(command_context, **kwargs):
    return run_raptor(command_context, **kwargs)
