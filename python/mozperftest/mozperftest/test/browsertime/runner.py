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
from pathlib import Path

from mozperftest.utils import install_package, get_output_dir
from mozperftest.test.noderunner import NodeRunner
from mozperftest.test.browsertime.visualtools import get_dependencies, xvfb


BROWSERTIME_SRC_ROOT = Path(__file__).parent


def matches(args, *flags):
    """Returns True if any argument matches any of the given flags

    Maybe with an argument.
    """

    for flag in flags:
        if flag in args or any(arg.startswith(flag + "=") for arg in args):
            return True
    return False


def extract_browser_name(args):
    "Extracts the browser name if any"
    # These are BT arguments, it's BT job to check them
    # here we just want to extract the browser name
    res = re.findall(r"(--browser|-b)[= ]([\w]+)", " ".join(args))
    if res == []:
        return None
    return res[0][-1]


class NodeException(Exception):
    pass


class BrowsertimeRunner(NodeRunner):
    """Runs a browsertime test."""

    name = "browsertime"
    activated = True
    user_exception = True

    arguments = {
        "cycles": {"type": int, "default": 1, "help": "Number of full cycles"},
        "iterations": {"type": int, "default": 1, "help": "Number of iterations"},
        "node": {"type": str, "default": None, "help": "Path to Node.js"},
        "geckodriver": {"type": str, "default": None, "help": "Path to geckodriver"},
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
        "xvfb": {"action": "store_true", "default": False, "help": "Use xvfb"},
        "no-window-recorder": {
            "action": "store_true",
            "default": False,
            "help": "Use the window recorder",
        },
        "viewport-size": {"type": str, "default": "1366x695", "help": "Viewport size"},
    }

    def __init__(self, env, mach_cmd):
        super(BrowsertimeRunner, self).__init__(env, mach_cmd)
        self.topsrcdir = mach_cmd.topsrcdir
        self._mach_context = mach_cmd._mach_context
        self.virtualenv_manager = mach_cmd.virtualenv_manager
        self._created_dirs = []
        self._test_script = None
        self._setup_helper = None
        self.get_binary_path = mach_cmd.get_binary_path

    @property
    def setup_helper(self):
        if self._setup_helper is not None:
            return self._setup_helper
        sys.path.append(str(Path(self.topsrcdir, "tools", "lint", "eslint")))
        import setup_helper

        self._setup_helper = setup_helper
        return self._setup_helper

    @property
    def artifact_cache_path(self):
        """Downloaded artifacts will be kept here."""
        # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
        return Path(self._mach_context.state_dir, "cache", "browsertime")

    @property
    def state_path(self):
        """Unpacked artifacts will be kept here."""
        # The convention is $MOZBUILD_STATE_PATH/$FEATURE.
        res = Path(self._mach_context.state_dir, "browsertime")
        os.makedirs(str(res), exist_ok=True)
        return res

    @property
    def browsertime_js(self):
        root = os.environ.get("BROWSERTIME", self.state_path)
        path = Path(root, "node_modules", "browsertime", "bin", "browsertime.js")
        if path.exists():
            os.environ["BROWSERTIME_JS"] = str(path)
        return path

    @property
    def visualmetrics_py(self):
        root = os.environ.get("BROWSERTIME", self.state_path)
        path = Path(
            root, "node_modules", "browsertime", "browsertime", "visualmetrics.py"
        )
        if path.exists():
            os.environ["VISUALMETRICS_PY"] = str(path)
        return path

    def _should_install(self):
        # If browsertime doesn't exist, install it
        if not self.visualmetrics_py.exists() or not self.browsertime_js.exists():
            return True

        # Browsertime exists, check if it's outdated
        with Path(BROWSERTIME_SRC_ROOT, "package.json").open() as new, Path(
            os.environ.get("BROWSERTIME", self.state_path),
            "node_modules",
            "browsertime",
            "package.json",
        ).open() as old:
            old_pkg = json.load(old)
            new_pkg = json.load(new)

        return not old_pkg["_from"].endswith(new_pkg["devDependencies"]["browsertime"])

    def setup(self):
        """Install browsertime and visualmetrics.py prerequisites and the Node.js package."""

        node = self.get_arg("node")
        if node is not None:
            os.environ["NODEJS"] = node

        super(BrowsertimeRunner, self).setup()
        install_url = self.get_arg("install-url")

        # installing Python deps on the fly
        visualmetrics = self.get_arg("visualmetrics", False)

        if visualmetrics:
            # installing Python deps on the fly
            for dep in get_dependencies():
                install_package(self.virtualenv_manager, dep, ignore_failure=True)

        # check if the browsertime package has been deployed correctly
        # for this we just check for the browsertime directory presence
        # we also make sure the visual metrics module is there *if*
        # we need it
        if not self._should_install() and not self.get_arg("clobber"):
            return

        # preparing ~/.mozbuild/browsertime
        for file in ("package.json", "package-lock.json"):
            src = BROWSERTIME_SRC_ROOT / file
            target = self.state_path / file
            # Overwrite the existing files
            shutil.copyfile(str(src), str(target))

        package_json_path = self.state_path / "package.json"

        if install_url is not None:
            self.info(
                "Updating browsertime node module version in {package_json_path} "
                "to {install_url}",
                install_url=install_url,
                package_json_path=str(package_json_path),
            )

            expr = r"/tarball/[a-f0-9]{40}$"
            if not re.search(expr, install_url):
                raise ValueError(
                    "New upstream URL does not end with {}: '{}'".format(
                        expr[:-1], install_url
                    )
                )

            with package_json_path.open() as f:
                existing_body = json.loads(
                    f.read(), object_pairs_hook=collections.OrderedDict
                )

            existing_body["devDependencies"]["browsertime"] = install_url
            updated_body = json.dumps(existing_body)
            with package_json_path.open("w") as f:
                f.write(updated_body)

        self._setup_node_packages(package_json_path)

    def _setup_node_packages(self, package_json_path):
        # Install the browsertime Node.js requirements.
        if not self.setup_helper.check_node_executables_valid():
            return

        should_clobber = self.get_arg("clobber")
        # To use a custom `geckodriver`, set
        # os.environ[b"GECKODRIVER_BASE_URL"] = bytes(url)
        # to an endpoint with binaries named like
        # https://github.com/sitespeedio/geckodriver/blob/master/install.js#L31.
        automation = "MOZ_AUTOMATION" in os.environ

        if automation:
            os.environ["CHROMEDRIVER_SKIP_DOWNLOAD"] = "true"
            os.environ["GECKODRIVER_SKIP_DOWNLOAD"] = "true"

        self.info(
            "Installing browsertime node module from {package_json}",
            package_json=str(package_json_path),
        )
        install_url = self.get_arg("install-url")

        self.setup_helper.package_setup(
            str(self.state_path),
            "browsertime",
            should_update=install_url is not None,
            should_clobber=should_clobber,
            no_optional=install_url or automation,
        )

    def extra_default_args(self, args=[]):
        # Add Mozilla-specific default arguments.  This is tricky because browsertime is quite
        # loose about arguments; repeat arguments are generally accepted but then produce
        # difficult to interpret type errors.
        extra_args = []

        # Default to Firefox.  Override with `-b ...` or `--browser=...`.
        if not matches(args, "-b", "--browser"):
            extra_args.extend(("-b", "firefox"))

        # Default to not collect HAR.  Override with `--skipHar=false`.
        if not matches(args, "--har", "--skipHar", "--gzipHar"):
            extra_args.append("--skipHar")

        extra_args.extend(["--viewPort", self.get_arg("viewport-size")])

        if not matches(args, "--android"):
            binary = self.get_arg("binary")
            if binary is not None:
                extra_args.extend(("--firefox.binaryPath", binary))
            else:
                # If --firefox.binaryPath is not specified, default to the objdir binary
                # Note: --firefox.release is not a real browsertime option, but it will
                #       silently ignore it instead and default to a release installation.
                if (
                    not matches(
                        args,
                        "--firefox.binaryPath",
                        "--firefox.release",
                        "--firefox.nightly",
                        "--firefox.beta",
                        "--firefox.developer",
                    )
                    and extract_browser_name(args) != "chrome"
                ):
                    extra_args.extend(("--firefox.binaryPath", self.get_binary_path()))

        geckodriver = self.get_arg("geckodriver")
        if geckodriver is not None:
            extra_args.extend(("--firefox.geckodriverPath", geckodriver))

        if extra_args:
            self.debug(
                "Running browsertime with extra default arguments: {extra_args}",
                extra_args=extra_args,
            )

        return extra_args

    def _android_args(self, metadata):
        app_name = self.get_arg("android-app-name")

        args_list = [
            "--android",
            # Work around a `selenium-webdriver` issue where Browsertime
            # fails to find a Firefox binary even though we're going to
            # actually do things on an Android device.
            "--firefox.binaryPath",
            self.node_path,
            "--firefox.android.package",
            app_name,
        ]
        activity = self.get_arg("android-activity")
        if activity is not None:
            args_list += ["--firefox.android.activity", activity]

        return args_list

    def run(self, metadata):
        self._test_script = metadata.script
        self.setup()
        cycles = self.get_arg("cycles", 1)
        for cycle in range(1, cycles + 1):

            # Build an output directory
            output = self.get_arg("output")
            if output is None:
                output = pathlib.Path(self.topsrcdir, "artifacts")
            result_dir = get_output_dir(output, f"browsertime-results-{cycle}")

            # Run the test cycle
            metadata.run_hook(
                "before_cycle", metadata, self.env, cycle, self._test_script
            )
            try:
                metadata = self._one_cycle(metadata, result_dir)
            finally:
                metadata.run_hook(
                    "after_cycle", metadata, self.env, cycle, self._test_script
                )
        return metadata

    def _one_cycle(self, metadata, result_dir):
        profile = self.get_arg("profile-directory")

        args = [
            "--resultDir",
            str(result_dir),
            "--firefox.profileTemplate",
            profile,
            "--iterations",
            str(self.get_arg("iterations")),
            self._test_script["filename"],
        ]

        # Set *all* prefs found in browser_prefs because
        # browsertime will override the ones found in firefox.profileTemplate
        # with its own defaults at `firefoxPreferences.js`
        # Using `--firefox.preference` ensures we override them.
        # see https://github.com/sitespeedio/browsertime/issues/1427
        browser_prefs = metadata.get_options("browser_prefs")
        for key, value in browser_prefs.items():
            args += ["--firefox.preference", f"{key}:{value}"]

        if self.get_arg("verbose"):
            args += ["-vvv"]

        # if the visualmetrics layer is activated, we want to feed it
        visualmetrics = self.get_arg("visualmetrics", False)
        if visualmetrics:
            args += ["--video", "true"]
            if not self.get_arg("no-window-recorder"):
                args += ["--firefox.windowRecorder", "true"]

        extra_options = self.get_arg("extra-options")
        if extra_options:
            for option in extra_options.split(","):
                option = option.strip()
                if not option:
                    continue
                option = option.split("=", 1)
                if len(option) != 2:
                    self.warning(
                        f"Skipping browsertime option {option} as it "
                        "is missing a name/value pairing. We expect options "
                        "to be formatted as: --browsertime-extra-options "
                        "'browserRestartTries=1,timeouts.browserStart=10'"
                    )
                    continue
                name, value = option
                args += ["--" + name, value]

        if self.get_arg("android"):
            args.extend(self._android_args(metadata))

        extra = self.extra_default_args(args=args)
        command = [str(self.browsertime_js)] + extra + args
        self.info("Running browsertime with this command %s" % " ".join(command))

        if visualmetrics and self.get_arg("xvfb"):
            with xvfb():
                exit_code = self.node(command)
        else:
            exit_code = self.node(command)

        if exit_code != 0:
            raise NodeException(exit_code)

        metadata.add_result(
            {"results": str(result_dir), "name": self._test_script["name"]}
        )

        return metadata
