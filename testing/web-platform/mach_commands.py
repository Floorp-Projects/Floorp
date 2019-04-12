# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the web-platform-tests test runner with mach.

from __future__ import absolute_import, unicode_literals, print_function

import os
import sys

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandProvider,
    Command,
)

from mach_commands_base import WebPlatformTestsRunner, create_parser_wpt


class WebPlatformTestsRunnerSetup(MozbuildObject):
    default_log_type = "mach"

    def __init__(self, *args, **kwargs):
        super(WebPlatformTestsRunnerSetup, self).__init__(*args, **kwargs)
        self._here = os.path.join(self.topsrcdir, 'testing', 'web-platform')
        kwargs["tests_root"] = os.path.join(self._here, "tests")
        sys.path.insert(0, kwargs["tests_root"])
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

    def kwargs_common(self, kwargs):
        tests_src_path = os.path.join(self._here, "tests")
        if kwargs["product"] == "fennec":
            # package_name may be non-fennec in the future
            package_name = kwargs["package_name"]
            if not package_name:
                package_name = self.substs["ANDROID_PACKAGE_NAME"]

            # Note that this import may fail in non-fennec trees
            from mozrunner.devices.android_device import verify_android_device, grant_runtime_permissions
            verify_android_device(self, install=True, verbose=False, xre=True, app=package_name)

            grant_runtime_permissions(self, package_name, kwargs["device_serial"])
            if kwargs["certutil_binary"] is None:
                kwargs["certutil_binary"] = os.path.join(os.environ.get('MOZ_HOST_BIN'), "certutil")

            if kwargs["install_fonts"] is None:
                kwargs["install_fonts"] = True

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topobjdir, '_tests', 'web-platform', 'wptrunner.local.ini')

        if kwargs["prefs_root"] is None:
            kwargs["prefs_root"] = os.path.join(self.topsrcdir, 'testing', 'profiles')

        if kwargs["stackfix_dir"] is None:
            kwargs["stackfix_dir"] = self.bindir

        if kwargs["exclude"] is None and kwargs["include"] is None and not sys.platform.startswith("linux"):
            kwargs["exclude"] = ["css"]

        if kwargs["ssl_type"] in (None, "pregenerated"):
            cert_root = os.path.join(tests_src_path, "tools", "certs")
            if kwargs["ca_cert_path"] is None:
                kwargs["ca_cert_path"] = os.path.join(cert_root, "cacert.pem")

            if kwargs["host_key_path"] is None:
                kwargs["host_key_path"] = os.path.join(cert_root, "web-platform.test.key")

            if kwargs["host_cert_path"] is None:
                kwargs["host_cert_path"] = os.path.join(cert_root, "web-platform.test.pem")

        if kwargs["lsan_dir"] is None:
            kwargs["lsan_dir"] = os.path.join(self.topsrcdir, "build", "sanitizers")

        if kwargs["reftest_screenshot"] is None:
            kwargs["reftest_screenshot"] = "fail"

        kwargs["capture_stdio"] = True

        return kwargs

    def kwargs_firefox(self, kwargs):
        import mozinfo
        from wptrunner import wptcommandline
        kwargs = self.kwargs_common(kwargs)

        if kwargs["binary"] is None:
            kwargs["binary"] = self.get_binary_path()

        if kwargs["certutil_binary"] is None:
            kwargs["certutil_binary"] = self.get_binary_path('certutil')

        if kwargs["webdriver_binary"] is None:
            kwargs["webdriver_binary"] = self.get_binary_path("geckodriver", validate_exists=False)


        if mozinfo.info["os"] == "win" and mozinfo.info["os_version"] == "6.1":
            # On Windows 7 --install-fonts fails, so fall back to a Firefox-specific codepath
            self.setup_fonts_firefox()
        else:
            kwargs["install_fonts"] = True

        kwargs = wptcommandline.check_args(kwargs)

        return kwargs

    def kwargs_wptrun(self, kwargs):
        from wptrunner import wptcommandline

        if kwargs["metadata_root"] is None:
            metadir = os.path.join(self._here, "products", kwargs["product"])
            if not os.path.exists(metadir):
                os.makedirs(metadir)
            kwargs["metadata_root"] = metadir

        src_manifest = os.path.join(self._here, "meta", "MANIFEST.json")
        dest_manifest = os.path.join(kwargs["metadata_root"], "MANIFEST.json")

        if not os.path.exists(dest_manifest) and os.path.exists(src_manifest):
            with open(src_manifest) as src, open(dest_manifest, "w") as dest:
                dest.write(src.read())

        from tools.wpt import run

        # Add additional kwargs consumed by the run frontend. Currently we don't
        # have a way to set these through mach
        kwargs["channel"] = None
        kwargs["prompt"] = True
        kwargs["install_browser"] = False

        try:
            kwargs = run.setup_wptrunner(run.virtualenv.Virtualenv(self.virtualenv_manager.virtualenv_root, False),
                                         **kwargs)
        except run.WptrunError as e:
            print(e.message, file=sys.stderr)
            sys.exit(1)

        return kwargs

    def setup_fonts_firefox(self):
        # Ensure the Ahem font is available
        if not sys.platform.startswith("darwin"):
            font_path = os.path.join(os.path.dirname(self.get_binary_path()), "fonts")
        else:
            font_path = os.path.join(os.path.dirname(self.get_binary_path()), os.pardir, "Resources", "res", "fonts")
        ahem_src = os.path.join(self.topsrcdir, "testing", "web-platform", "tests", "fonts", "Ahem.ttf")
        ahem_dest = os.path.join(font_path, "Ahem.ttf")
        if not os.path.exists(ahem_dest) and os.path.exists(ahem_src):
            with open(ahem_src, "rb") as src, open(ahem_dest, "wb") as dest:
                dest.write(src.read())



class WebPlatformTestsUpdater(MozbuildObject):
    """Update web platform tests."""
    def setup_logging(self, **kwargs):
        import update
        return update.setup_logging(kwargs, {"mach": sys.stdout})

    def update_manifest(self, logger, **kwargs):
        import manifestupdate
        return manifestupdate.run(logger=logger,
                                  src_root=self.topsrcdir,
                                  obj_root=self.topobjdir,
                                  **kwargs)

    def run_update(self, logger, **kwargs):
        import update
        from update import updatecommandline

        self.update_manifest(logger, **kwargs)

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topobjdir, '_tests', 'web-platform', 'wptrunner.local.ini')
        if kwargs["product"] is None:
            kwargs["product"] = "firefox"

        kwargs["store_state"] = False

        kwargs = updatecommandline.check_args(kwargs)

        try:
            update.run_update(logger, **kwargs)
        except Exception:
            import pdb
            import traceback
            traceback.print_exc()
#            pdb.post_mortem()


def create_parser_update():
    from update import updatecommandline
    return updatecommandline.create_parser()


def create_parser_create():
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--no-editor", action="store_true",
                   help="Don't try to open the test in an editor")
    p.add_argument("-e", "--editor", action="store", help="Editor to use")
    p.add_argument("--long-timeout", action="store_true",
                   help="Test should be given a long timeout (typically 60s rather than 10s, but varies depending on environment)")
    p.add_argument("--overwrite", action="store_true",
                   help="Allow overwriting an existing test file")
    p.add_argument("-r", "--reftest", action="store_true",
                   help="Create a reftest rather than a testharness (js) test"),
    p.add_argument("-m", "--reference", dest="ref", help="Path to the reference file")
    p.add_argument("--mismatch", action="store_true",
                   help="Create a mismatch reftest")
    p.add_argument("--wait", action="store_true",
                   help="Create a reftest that waits until takeScreenshot() is called")
    p.add_argument("path", action="store", help="Path to the test file")
    return p


def create_parser_manifest_update():
    import manifestupdate
    return manifestupdate.create_parser()


def create_parser_metadata_summary():
    import metasummary
    return metasummary.create_parser()


def create_parser_serve():
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                    "tests", "tools")))
    import serve
    return serve.serve.get_parser()


@CommandProvider
class MachCommands(MachCommandBase):
    def setup(self):
        self._activate_virtualenv()

    @Command("web-platform-tests",
             category="testing",
             conditions=[conditions.is_firefox_or_android],
             description="Run web-platform-tests.",
             parser=create_parser_wpt)
    def run_web_platform_tests(self, **params):
        self.setup()
        if conditions.is_android(self) and params["product"] != "fennec":
            if params["product"] is None:
                params["product"] = "fennec"
            else:
                raise ValueError("Must specify --product=fennec in Android environment.")
        if "test_objects" in params:
            for item in params["test_objects"]:
                params["include"].append(item["name"])
            del params["test_objects"]

        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        wpt_runner = WebPlatformTestsRunner(wpt_setup)

        if params["log_mach_screenshot"] is None:
            params["log_mach_screenshot"] = True

        logger = wpt_runner.setup_logging(**params)

        return wpt_runner.run(logger, **params)

    @Command("wpt",
             category="testing",
             conditions=[conditions.is_firefox_or_android],
             description="Run web-platform-tests.",
             parser=create_parser_wpt)
    def run_wpt(self, **params):
        return self.run_web_platform_tests(**params)

    @Command("web-platform-tests-update",
             category="testing",
             description="Update web-platform-test metadata.",
             parser=create_parser_update)
    def update_web_platform_tests(self, **params):
        self.setup()
        self.virtualenv_manager.install_pip_package('html5lib==1.0.1')
        self.virtualenv_manager.install_pip_package('ujson')
        self.virtualenv_manager.install_pip_package('requests')

        wpt_updater = self._spawn(WebPlatformTestsUpdater)
        logger = wpt_updater.setup_logging(**params)
        return wpt_updater.run_update(logger, **params)

    @Command("wpt-update",
             category="testing",
             description="Update web-platform-test metadata.",
             parser=create_parser_update)
    def update_wpt(self, **params):
        return self.update_web_platform_tests(**params)

    @Command("wpt-manifest-update",
             category="testing",
             description="Update web-platform-test manifests.",
             parser=create_parser_manifest_update)
    def wpt_manifest_update(self, **params):
        self.setup()
        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        wpt_runner = WebPlatformTestsRunner(wpt_setup)
        logger = wpt_runner.setup_logging(**params)
        logger.warning("The wpt manifest is now automatically updated, "
                       "so running this command is usually unnecessary")
        return 0 if wpt_runner.update_manifest(logger, **params) else 1

    @Command("wpt-serve",
             category="testing",
             description="Run the wpt server",
             parser=create_parser_serve)
    def wpt_manifest_serve(self, **params):
        self.setup()
        import logging
        logger = logging.getLogger("web-platform-tests")
        logger.addHandler(logging.StreamHandler(sys.stdout))
        import serve
        return 0 if serve.serve.run(**params) else 1

    @Command("wpt-metadata-summary",
             category="testing",
             description="Create a json summary of the wpt metadata",
             parser=create_parser_metadata_summary)
    def wpt_summary(self, **params):
        import metasummary
        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        return metasummary.run(wpt_setup.topsrcdir, wpt_setup.topobjdir, **params)
