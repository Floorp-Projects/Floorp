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

    def kwargs_firefox(self, kwargs):
        from wptrunner import wptcommandline

        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topsrcdir, 'testing', 'web-platform', 'wptrunner.ini')

        if kwargs["binary"] is None:
            kwargs["binary"] = self.get_binary_path()

        if kwargs["prefs_root"] is None:
            kwargs["prefs_root"] = os.path.join(self.topobjdir, '_tests', 'web-platform', "prefs")

        if kwargs["certutil_binary"] is None:
            kwargs["certutil_binary"] = self.get_binary_path('certutil')

        if kwargs["stackfix_dir"] is None:
            kwargs["stackfix_dir"] = os.path.split(
                self.get_binary_path(validate_exists=False))[0]

        here = os.path.split(__file__)[0]

        if kwargs["ssl_type"] in (None, "pregenerated"):
            if kwargs["ca_cert_path"] is None:
                kwargs["ca_cert_path"] = os.path.join(here, "certs", "cacert.pem")

            if kwargs["host_key_path"] is None:
                kwargs["host_key_path"] = os.path.join(here, "certs", "web-platform.test.key")

            if kwargs["host_cert_path"] is None:
                kwargs["host_cert_path"] = os.path.join(here, "certs", "web-platform.test.pem")

        kwargs["capture_stdio"] = True

        if kwargs["webdriver_binary"] is None:
            kwargs["webdriver_binary"] = self.get_binary_path("geckodriver", validate_exists=False)

        kwargs = wptcommandline.check_args(kwargs)

    def kwargs_wptrun(self, kwargs):
        from wptrunner import wptcommandline
        here = os.path.join(self.topsrcdir, 'testing', 'web-platform')

        sys.path.insert(0, os.path.join(here, "tests", "tools"))

        import wptrun

        product = kwargs["product"]

        setup_func = {
            "chrome": wptrun.setup_chrome,
            "edge": wptrun.setup_edge,
            "servo": wptrun.setup_servo,
        }[product]

        try:
            wptrun.check_environ(product)

            setup_func(wptrun.virtualenv.Virtualenv(self.virtualenv_manager.virtualenv_root),
                       kwargs,
                       True)
        except wptrun.WptrunError as e:
            print(e.message, file=sys.stderr)
            sys.exit(1)

        kwargs["tests_root"] = os.path.join(here, "tests")

        if kwargs["metadata_root"] is None:
            metadir = os.path.join(here, "products", kwargs["product"])
            if not os.path.exists(metadir):
                os.makedirs(metadir)
            kwargs["metadata_root"] = metadir

        src_manifest = os.path.join(here, "meta", "MANIFEST.json")
        dest_manifest = os.path.join(kwargs["metadata_root"], "MANIFEST.json")

        if not os.path.exists(dest_manifest) and os.path.exists(src_manifest):
            with open(src_manifest) as src, open(dest_manifest, "w") as dest:
                dest.write(src.read())

        kwargs = wptcommandline.check_args(kwargs)


class WebPlatformTestsUpdater(MozbuildObject):
    """Update web platform tests."""
    def run_update(self, **kwargs):
        import update
        from update import updatecommandline

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topsrcdir, 'testing', 'web-platform', 'wptrunner.ini')
        if kwargs["product"] is None:
            kwargs["product"] = "firefox"

        if kwargs["sync"]:
            if not kwargs["exclude"]:
                kwargs["exclude"] = ["css/*"]
            if not kwargs["include"]:
                kwargs["include"] = ["css/css-timing-1/*", "css/css-animations-1/*", "css/css-transitions-1/*"]


        kwargs = updatecommandline.check_args(kwargs)
        logger = update.setup_logging(kwargs, {"mach": sys.stdout})

        try:
            update.run_update(logger, **kwargs)
        except Exception:
            import pdb
            import traceback
            traceback.print_exc()
#            pdb.post_mortem()

class WebPlatformTestsReduce(WebPlatformTestsRunner):

    def run_reduce(self, **kwargs):
        from wptrunner import reduce

        self.setup_kwargs(kwargs)

        kwargs["capture_stdio"] = True
        logger = reduce.setup_logging(kwargs, {"mach": sys.stdout})
        tests = reduce.do_reduce(**kwargs)

        if not tests:
            logger.warning("Test was not unstable")

        for item in tests:
            logger.info(item.id)

class WebPlatformTestsCreator(MozbuildObject):
    template_prefix = """<!doctype html>
%(documentElement)s<meta charset=utf-8>
"""
    template_long_timeout = "<meta name=timeout content=long>\n"

    template_body_th = """<title></title>
<script src=/resources/testharness.js></script>
<script src=/resources/testharnessreport.js></script>
<script>

</script>
"""

    template_body_reftest = """<title></title>
<link rel=%(match)s href=%(ref)s>
"""

    template_body_reftest_wait = """<script src="/common/reftest-wait.js"></script>
"""

    def rel_path(self, path):
        if path is None:
            return

        abs_path = os.path.normpath(os.path.abspath(path))
        return os.path.relpath(abs_path, self.topsrcdir)

    def rel_url(self, rel_path):
        upstream_path = os.path.join("testing", "web-platform", "tests")
        local_path = os.path.join("testing", "web-platform", "mozilla", "tests")

        if rel_path.startswith(upstream_path):
            return rel_path[len(upstream_path):].replace(os.path.sep, "/")
        elif rel_path.startswith(local_path):
            return "/_mozilla" + rel_path[len(local_path):].replace(os.path.sep, "/")
        else:
            return None

    def run_create(self, context, **kwargs):
        import subprocess

        path = self.rel_path(kwargs["path"])
        ref_path = self.rel_path(kwargs["ref"])

        if kwargs["ref"]:
            kwargs["reftest"] = True

        if self.rel_url(path) is None:
            print("""Test path %s is not in wpt directories:
testing/web-platform/tests for tests that may be shared
testing/web-platform/mozilla/tests for Gecko-only tests""" % path)
            return 1

        if ref_path and self.rel_url(ref_path) is None:
            print("""Reference path %s is not in wpt directories:
testing/web-platform/tests for tests that may be shared
            testing/web-platform/mozilla/tests for Gecko-only tests""" % ref_path)
            return 1


        if os.path.exists(path) and not kwargs["overwrite"]:
            print("Test path already exists, pass --overwrite to replace")
            return 1

        if kwargs["mismatch"] and not kwargs["reftest"]:
            print("--mismatch only makes sense for a reftest")
            return 1

        if kwargs["wait"] and not kwargs["reftest"]:
            print("--wait only makes sense for a reftest")
            return 1

        args = {"documentElement": "<html class=reftest-wait>\n" if kwargs["wait"] else ""}
        template = self.template_prefix % args
        if kwargs["long_timeout"]:
            template += self.template_long_timeout

        if kwargs["reftest"]:
            args = {"match": "match" if not kwargs["mismatch"] else "mismatch",
                    "ref": self.rel_url(ref_path) if kwargs["ref"] else '""'}
            template += self.template_body_reftest % args
            if kwargs["wait"]:
                template += self.template_body_reftest_wait
        else:
            template += self.template_body_th
        try:
            os.makedirs(os.path.dirname(path))
        except OSError:
            pass
        with open(path, "w") as f:
            f.write(template)

        if kwargs["no_editor"]:
            editor = None
        elif kwargs["editor"]:
            editor = kwargs["editor"]
        elif "VISUAL" in os.environ:
            editor = os.environ["VISUAL"]
        elif "EDITOR" in os.environ:
            editor = os.environ["EDITOR"]
        else:
            editor = None

        proc = None
        if editor:
            proc = subprocess.Popen("%s %s" % (editor, path), shell=True)

        if not kwargs["no_run"]:
            p = create_parser_wpt()
            wpt_kwargs = vars(p.parse_args(["--manifest-update", path]))
            context.commands.dispatch("web-platform-tests", context, **wpt_kwargs)

        if proc:
            proc.wait()


class WPTManifestUpdater(MozbuildObject):
    def run_update(self, check_clean=False, rebuild=False, **kwargs):
        import manifestupdate
        from wptrunner import wptlogging
        logger = wptlogging.setup(kwargs, {"mach": sys.stdout})
        wpt_dir = os.path.abspath(os.path.join(self.topsrcdir, 'testing', 'web-platform'))
        manifestupdate.update(logger, wpt_dir, check_clean, rebuild)


def create_parser_update():
    from update import updatecommandline
    return updatecommandline.create_parser()

def create_parser_reduce():
    from wptrunner import wptcommandline
    return wptcommandline.create_parser_reduce()

def create_parser_create():
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--no-editor", action="store_true",
                   help="Don't try to open the test in an editor")
    p.add_argument("-e", "--editor", action="store", help="Editor to use")
    p.add_argument("--no-run", action="store_true",
                   help="Don't try to update the wpt manifest or open the test in a browser")
    p.add_argument("--long-timeout", action="store_true",
                   help="Test should be given a long timeout (typically 60s rather than 10s, but varies depending on environment)")
    p.add_argument("--overwrite", action="store_true",
                   help="Allow overwriting an existing test file")
    p.add_argument("-r", "--reftest", action="store_true",
                   help="Create a reftest rather than a testharness (js) test"),
    p.add_argument("-ref", "--reference", dest="ref", help="Path to the reference file")
    p.add_argument("--mismatch", action="store_true",
                   help="Create a mismatch reftest")
    p.add_argument("--wait", action="store_true",
                   help="Create a reftest that waits until takeScreenshot() is called")
    p.add_argument("path", action="store", help="Path to the test file")
    return p


def create_parser_manifest_update():
    import manifestupdate
    return manifestupdate.create_parser()


@CommandProvider
class MachCommands(MachCommandBase):
    def setup(self):
        self._activate_virtualenv()

    @Command("web-platform-tests",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_wpt)
    def run_web_platform_tests(self, **params):
        self.setup()

        if "test_objects" in params:
            for item in params["test_objects"]:
                params["include"].append(item["name"])
            del params["test_objects"]

        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        wpt_runner = WebPlatformTestsRunner(wpt_setup)
        return wpt_runner.run(**params)

    @Command("wpt",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_wpt)
    def run_wpt(self, **params):
        return self.run_web_platform_tests(**params)

    @Command("web-platform-tests-update",
             category="testing",
             parser=create_parser_update)
    def update_web_platform_tests(self, **params):
        self.setup()
        self.virtualenv_manager.install_pip_package('html5lib==0.99')
        self.virtualenv_manager.install_pip_package('requests')
        wpt_updater = self._spawn(WebPlatformTestsUpdater)
        return wpt_updater.run_update(**params)

    @Command("wpt-update",
             category="testing",
             parser=create_parser_update)
    def update_wpt(self, **params):
        return self.update_web_platform_tests(**params)

    @Command("web-platform-tests-reduce",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_reduce)
    def unstable_web_platform_tests(self, **params):
        self.setup()
        wpt_reduce = self._spawn(WebPlatformTestsReduce)
        return wpt_reduce.run_reduce(**params)

    @Command("wpt-reduce",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_reduce)
    def unstable_wpt(self, **params):
        return self.unstable_web_platform_tests(**params)

    @Command("web-platform-tests-create",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_create)
    def create_web_platform_test(self, **params):
        self.setup()
        wpt_creator = self._spawn(WebPlatformTestsCreator)
        wpt_creator.run_create(self._mach_context, **params)

    @Command("wpt-create",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_create)
    def create_wpt(self, **params):
        return self.create_web_platform_test(**params)

    @Command("wpt-manifest-update",
             category="testing",
             parser=create_parser_manifest_update)
    def wpt_manifest_update(self, **params):
        self.setup()
        wpt_manifest_updater = self._spawn(WPTManifestUpdater)
        return wpt_manifest_updater.run_update(**params)
