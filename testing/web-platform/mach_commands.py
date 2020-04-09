# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the web-platform-tests test runner with mach.

from __future__ import absolute_import, unicode_literals, print_function

import os
import sys

from six import iteritems

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
        if kwargs["product"] == "firefox_android":
            # package_name may be different in the future
            package_name = kwargs["package_name"]
            if not package_name:
                kwargs["package_name"] = package_name = "org.mozilla.geckoview.test"

            # Note that this import may fail in non-firefox-for-android trees
            from mozrunner.devices.android_device import (verify_android_device, InstallIntent)
            install = InstallIntent.NO if kwargs.pop('no_install') else InstallIntent.YES
            verify_android_device(self, install=install, verbose=False, xre=True, app=package_name)

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

        if kwargs["install_fonts"] is None:
            kwargs["install_fonts"] = True

        if kwargs["install_fonts"] and mozinfo.info["os"] == "win" and mozinfo.info["os_version"] == "6.1":
            # On Windows 7 --install-fonts fails, so fall back to a Firefox-specific codepath
            self.setup_fonts_firefox()
            kwargs["install_fonts"] = False

        if kwargs["preload_browser"] is None:
            kwargs["preload_browser"] = False

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

        # Install the deps
        # We do this explicitly to avoid calling pip with options that aren't
        # supported in the in-tree version
        wptrunner_path = os.path.join(self._here, "tests", "tools", "wptrunner")
        browser_cls = run.product_setup[kwargs["product"]].browser_cls
        requirements = ["requirements.txt"]
        if hasattr(browser_cls, "requirements"):
            requirements.append(browser_cls.requirements)

        for filename in requirements:
            path = os.path.join(wptrunner_path, filename)
            if os.path.exists(path):
                self.virtualenv_manager.install_pip_requirements(path, require_hashes=False)

        venv = run.virtualenv.Virtualenv(self.virtualenv_manager.virtualenv_root,
                                         skip_virtualenv_setup=True)
        try:
            kwargs = run.setup_wptrunner(venv, **kwargs)
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


class WebPlatformTestsUnittestRunner(MozbuildObject):
    def run(self, **kwargs):
        import unittestrunner
        return unittestrunner.run(self.topsrcdir, **kwargs)


class WebPlatformTestsTestPathsRunner(MozbuildObject):
    """Update web platform tests."""
    def run(self, **kwargs):
        sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                        "tests", "tools")))
        from wptrunner import wptcommandline
        from manifest import testpaths
        import manifestupdate

        import logging
        logger = logging.getLogger("web-platform-tests")

        src_root = self.topsrcdir
        obj_root = self.topobjdir
        src_wpt_dir = os.path.join(src_root, "testing", "web-platform")

        config_path = manifestupdate.generate_config(logger, src_root, src_wpt_dir,
                                                     os.path.join(obj_root, "_tests", "web-platform"),
                                                     False)

        test_paths = wptcommandline.get_test_paths(wptcommandline.config.read(config_path))
        results = {}
        for url_base, paths in iteritems(test_paths):
            if "manifest_path" not in paths:
                paths["manifest_path"] = os.path.join(paths["metadata_path"],
                                                      "MANIFEST.json")
            results.update(
                testpaths.get_paths(path=paths["manifest_path"],
                                    src_root=src_root,
                                    tests_root=paths["tests_path"],
                                    update=kwargs["update"],
                                    rebuild=kwargs["rebuild"],
                                    url_base=url_base,
                                    cache_root=kwargs["cache_root"],
                                    test_ids=kwargs["test_ids"]))
        testpaths.write_output(results, kwargs["json"])
        return True


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


def create_parser_metadata_merge():
    import metamerge
    return metamerge.get_parser()

def create_parser_serve():
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                    "tests", "tools")))
    import serve
    return serve.serve.get_parser()


def create_parser_unittest():
    import unittestrunner
    return unittestrunner.get_parser()


def create_parser_testpaths():
    import argparse
    from mozboot.util import get_state_dir
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--no-update", dest="update", action="store_false", default=True,
        help="Don't update manifest before continuing")
    parser.add_argument(
        "-r", "--rebuild", action="store_true", default=False,
        help="Force a full rebuild of the manifest.")
    parser.add_argument(
        "--cache-root", action="store", default=os.path.join(get_state_dir(), "cache", "wpt"),
        help="Path in which to store any caches (default <tests_root>/.wptcache/)")
    parser.add_argument(
        "test_ids", action="store", nargs="+",
        help="Test ids for which to get paths")
    parser.add_argument(
        "--json", action="store_true", default=False,
        help="Output as JSON")
    return parser


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
        if conditions.is_android(self) and params["product"] != "firefox_android":
            if params["product"] is None:
                params["product"] = "firefox_android"
        if "test_objects" in params:
            for item in params["test_objects"]:
                params["include"].append(item["name"])
            del params["test_objects"]
        if params.get('debugger', None):
            import mozdebug
            if not mozdebug.get_debugger_info(params.get('debugger')):
                sys.exit(1)

        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        wpt_setup._mach_context = self._mach_context
        wpt_runner = WebPlatformTestsRunner(wpt_setup)

        logger = wpt_runner.setup_logging(**params)

        if conditions.is_android(self) and params["product"] != "firefox_android":
            logger.warning("Must specify --product=firefox_android in Android environment.")

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

    @Command("wpt-metadata-merge",
             category="testing",
             parser=create_parser_metadata_merge)
    def wpt_meta_merge(self, **params):
        import metamerge
        if params["dest"] is None:
            params["dest"] = params["current"]
        return metamerge.run(**params)

    @Command("wpt-unittest",
             category="testing",
             description="Run the wpt tools and wptrunner unit tests",
             parser=create_parser_unittest)
    def wpt_unittest(self, **params):
        self.setup()
        self.virtualenv_manager.install_pip_package('tox')
        runner = self._spawn(WebPlatformTestsUnittestRunner)
        return 0 if runner.run(**params) else 1

    @Command("wpt-test-paths",
             category="testing",
             description="Get a mapping from test ids to files",
             parser=create_parser_testpaths)
    def wpt_test_paths(self, **params):
        runner = self._spawn(WebPlatformTestsTestPathsRunner)
        runner.run(**params)
        return 0
