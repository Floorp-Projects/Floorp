# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import collections
import json
import os
import pathlib
import sys
import re
import shutil

from mozbuild.util import mkdir
import mozpack.path as mozpath

from mozperftest.browser.noderunner import NodeRunner
from mozperftest.browser.browsertime.setup import (
    system_prerequisites,
    append_system_env,
)


AUTOMATION = "MOZ_AUTOMATION" in os.environ
BROWSERTIME_SRC_ROOT = os.path.dirname(__file__)
PILLOW_VERSION = "6.0.0"
PYSSIM_VERSION = "0.4"


class BrowsertimeRunner(NodeRunner):
    """Runs a browsertime test.
    """

    name = "browsertime"
    activated = True

    arguments = {
        "cycles": {"type": int, "default": 1, "help": "Number of full cycles"},
        "iterations": {"type": int, "default": 1, "help": "Number of iterations"},
        "binary": {
            "type": str,
            "default": None,
            "help": "Path to the desktop browser, or Android app name.",
        },
        "clobber": {
            "action": "store_true",
            "default": False,
            "help": "Force-update the installation.",
        },
        "install-url": {
            "type": str,
            "default": None,
            "help": "Use this URL as the install url.",
        },
        "extra-options": {
            "type": str,
            "default": "",
            "help": "Extra options passed to browsertime.js",
        },
    }

    def __init__(self, env, mach_cmd):
        super(BrowsertimeRunner, self).__init__(env, mach_cmd)
        self.topsrcdir = mach_cmd.topsrcdir
        self._mach_context = mach_cmd._mach_context
        self.virtualenv_manager = mach_cmd.virtualenv_manager
        self._created_dirs = []

    @property
    def artifact_cache_path(self):
        """Downloaded artifacts will be kept here."""
        # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
        return mozpath.join(self._mach_context.state_dir, "cache", "browsertime")

    @property
    def state_path(self):
        """Unpacked artifacts will be kept here."""
        # The convention is $MOZBUILD_STATE_PATH/$FEATURE.
        res = mozpath.join(self._mach_context.state_dir, "browsertime")
        mkdir(res)
        return res

    @property
    def browsertime_js(self):
        root = os.environ.get("BROWSERTIME", self.state_path)
        return os.path.join(
            root, "node_modules", "browsertime", "bin", "browsertime.js"
        )

    def _need_install(self, package):
        from pip._internal.req.constructors import install_req_from_line

        req = install_req_from_line("Pillow")
        req.check_if_exists(use_user_site=False)
        if req.satisfied_by is None:
            return True
        venv_site_lib = os.path.abspath(
            os.path.join(self.virtualenv_manager.bin_path, "..", "lib")
        )
        site_packages = os.path.abspath(req.satisfied_by.location)
        return not site_packages.startswith(venv_site_lib)

    def setup(self):
        """Install browsertime and visualmetrics.py prerequisites and the Node.js package.
        """
        super(BrowsertimeRunner, self).setup()
        install_url = self.get_arg("install-url")

        # installing Python deps on the fly
        for dep in ("Pillow==%s" % PILLOW_VERSION, "pyssim==%s" % PYSSIM_VERSION):
            if self._need_install(dep):
                self.virtualenv_manager._run_pip(["install", dep])

        # check if the browsertime package has been deployed correctly
        # for this we just check for the browsertime directory presence
        if os.path.exists(self.browsertime_js):
            return

        if install_url is None:
            system_prerequisites(
                self.state_path, self.artifact_cache_path, self.log, self.info
            )

        # preparing ~/.mozbuild/browsertime
        for file in ("package.json", "package-lock.json"):
            src = mozpath.join(BROWSERTIME_SRC_ROOT, file)
            target = mozpath.join(self.state_path, file)
            if not os.path.exists(target):
                shutil.copyfile(src, target)

        package_json_path = mozpath.join(self.state_path, "package.json")

        if install_url is not None:
            self.info(
                "Updating browsertime node module version in {package_json_path} "
                "to {install_url}",
                install_url=install_url,
                package_json_path=package_json_path,
            )

            if not re.search("/tarball/[a-f0-9]{40}$", install_url):
                raise ValueError(
                    "New upstream URL does not end with /tarball/[a-f0-9]{40}: '{}'".format(
                        install_url
                    )
                )

            with open(package_json_path) as f:
                existing_body = json.loads(
                    f.read(), object_pairs_hook=collections.OrderedDict
                )

            existing_body["devDependencies"]["browsertime"] = install_url
            updated_body = json.dumps(existing_body)
            with open(package_json_path, "w") as f:
                f.write(updated_body)

        self._setup_node_packages(package_json_path)

    def _setup_node_packages(self, package_json_path):
        # Install the browsertime Node.js requirements.
        sys.path.append(mozpath.join(self.topsrcdir, "tools", "lint", "eslint"))
        import setup_helper

        if not setup_helper.check_node_executables_valid():
            return

        should_clobber = self.get_arg("clobber")
        # To use a custom `geckodriver`, set
        # os.environ[b"GECKODRIVER_BASE_URL"] = bytes(url)
        # to an endpoint with binaries named like
        # https://github.com/sitespeedio/geckodriver/blob/master/install.js#L31.
        if AUTOMATION:
            os.environ["CHROMEDRIVER_SKIP_DOWNLOAD"] = "true"
            os.environ["GECKODRIVER_SKIP_DOWNLOAD"] = "true"

        self.info(
            "Installing browsertime node module from {package_json}",
            package_json=package_json_path,
        )
        install_url = self.get_arg("install-url")

        setup_helper.package_setup(
            self.state_path,
            "browsertime",
            should_update=install_url is not None,
            should_clobber=should_clobber,
            no_optional=install_url or AUTOMATION,
        )

    def append_env(self, append_path=True):
        env = super(BrowsertimeRunner, self).append_env(append_path)
        return append_system_env(env, self.state_path, append_path)

    def extra_default_args(self, args=[]):
        # Add Mozilla-specific default arguments.  This is tricky because browsertime is quite
        # loose about arguments; repeat arguments are generally accepted but then produce
        # difficult to interpret type errors.

        def extract_browser_name(args):
            "Extracts the browser name if any"
            # These are BT arguments, it's BT job to check them
            # here we just want to extract the browser name
            res = re.findall("(--browser|-b)[= ]([\w]+)", " ".join(args))
            if res == []:
                return None
            return res[0][-1]

        def matches(args, *flags):
            "Return True if any argument matches any of the given flags (maybe with an argument)."
            for flag in flags:
                if flag in args or any(arg.startswith(flag + "=") for arg in args):
                    return True
            return False

        extra_args = []

        # Default to Firefox.  Override with `-b ...` or `--browser=...`.
        specifies_browser = matches(args, "-b", "--browser")
        if not specifies_browser:
            extra_args.extend(("-b", "firefox"))

        # Default to not collect HAR.  Override with `--skipHar=false`.
        specifies_har = matches(args, "--har", "--skipHar", "--gzipHar")
        if not specifies_har:
            extra_args.append("--skipHar")

        if not matches(args, "--android"):
            # If --firefox.binaryPath is not specified, default to the objdir binary
            # Note: --firefox.release is not a real browsertime option, but it will
            #       silently ignore it instead and default to a release installation.
            specifies_binaryPath = matches(
                args,
                "--firefox.binaryPath",
                "--firefox.release",
                "--firefox.nightly",
                "--firefox.beta",
                "--firefox.developer",
            )

            if not specifies_binaryPath:
                specifies_binaryPath = extract_browser_name(args) == "chrome"

            if not specifies_binaryPath:
                try:
                    extra_args.extend(("--firefox.binaryPath", self.get_binary_path()))
                except Exception:
                    print(
                        "Please run |./mach build| "
                        "or specify a Firefox binary with --firefox.binaryPath."
                    )
                    return 1

        if extra_args:
            self.debug(
                "Running browsertime with extra default arguments: {extra_args}",
                extra_args=extra_args,
            )

        return extra_args

    def browsertime_android(self, metadata):
        app_name = self.get_arg("android-app-name")

        args_list = [
            # "--browser", "firefox",
            "--android",
            # Work around a `selenium-webdriver` issue where Browsertime
            # fails to find a Firefox binary even though we're going to
            # actually do things on an Android device.
            # "--firefox.binaryPath", self.mach_cmd.get_binary_path(),
            "--firefox.android.package",
            app_name,
        ]
        activity = self.get_arg("android-activity")
        if activity is not None:
            args_list += ["--firefox.android.activity", activity]

        return args_list

    def __call__(self, metadata):
        self.setup()
        cycles = self.get_arg("cycles", 1)
        for cycle in range(1, cycles + 1):
            metadata.run_hook("before_cycle", cycle=cycle)
            try:
                metadata = self._one_cycle(metadata)
            finally:
                metadata.run_hook("after_cycle", cycle=cycle)
        return metadata

    def _one_cycle(self, metadata):
        profile = self.get_arg("profile-directory")
        test_script = self.get_arg("tests")[0]
        output = self.get_arg("output")
        if output is not None:
            p = pathlib.Path(output)
            p = p / "browsertime-results"
            result_dir = str(p.resolve())
        else:
            result_dir = os.path.join(
                self.topsrcdir, "artifacts", "browsertime-results"
            )
        if not os.path.exists(result_dir):
            os.makedirs(result_dir, exist_ok=True)

        args = [
            "--resultDir",
            result_dir,
            "--firefox.profileTemplate",
            profile,
            "--iterations",
            str(self.get_arg("iterations")),
            test_script,
        ]

        if self.get_arg("verbose"):
            args += ["-vvv"]

        extra_options = self.get_arg("extra-options")
        if extra_options:
            for option in extra_options.split(","):
                option = option.strip()
                if not option:
                    continue
                option = option.split("=")
                if len(option) != 2:
                    continue
                name, value = option
                args += ["--" + name, value]

        firefox_args = ["--firefox.developer"]
        if self.get_arg("android"):
            args.extend(self.browsertime_android(metadata))

        extra = self.extra_default_args(args=firefox_args)
        command = [self.browsertime_js] + extra + args
        self.info("Running browsertime with this command %s" % " ".join(command))
        self.node(command)
        metadata.set_result(result_dir)
        return metadata
