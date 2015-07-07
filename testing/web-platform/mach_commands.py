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

# This should probably be consolidated with similar classes in other test
# runners.
class InvalidTestPathError(Exception):
    """Exception raised when the test path is not valid."""

class WebPlatformTestsRunner(MozbuildObject):
    """Run web platform tests."""

    def setup_kwargs(self, kwargs):
        from wptrunner import wptcommandline

        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topsrcdir, 'testing', 'web-platform', 'wptrunner.ini')

        if kwargs["binary"] is None:
            kwargs["binary"] = self.get_binary_path('app')

        if kwargs["prefs_root"] is None:
            kwargs["prefs_root"] = os.path.join(self.topobjdir, '_tests', 'web-platform', "prefs")

        if kwargs["certutil_binary"] is None:
            kwargs["certutil_binary"] = self.get_binary_path('certutil')

        here = os.path.split(__file__)[0]

        if kwargs["ssl_type"] in (None, "pregenerated"):
            if kwargs["ca_cert_path"] is None:
                kwargs["ca_cert_path"] = os.path.join(here, "certs", "cacert.pem")

            if kwargs["host_key_path"] is None:
                kwargs["host_key_path"] = os.path.join(here, "certs", "web-platform.test.key")

            if kwargs["host_cert_path"] is None:
                kwargs["host_cert_path"] = os.path.join(here, "certs", "web-platform.test.pem")

        kwargs["capture_stdio"] = True

        kwargs = wptcommandline.check_args(kwargs)

    def run_tests(self, **kwargs):
        from wptrunner import wptrunner

        self.setup_kwargs(kwargs)

        logger = wptrunner.setup_logging(kwargs, {"mach": sys.stdout})
        result = wptrunner.run_tests(**kwargs)

        return int(not result)

    def list_test_groups(self, **kwargs):
        from wptrunner import wptrunner

        self.setup_kwargs(kwargs)

        wptrunner.list_test_groups(**kwargs)

class WebPlatformTestsUpdater(MozbuildObject):
    """Update web platform tests."""
    def run_update(self, **kwargs):
        import update
        from update import updatecommandline

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topsrcdir, 'testing', 'web-platform', 'wptrunner.ini')
        updatecommandline.check_args(kwargs)
        logger = update.setup_logging(kwargs, {"mach": sys.stdout})

        try:
            update.run_update(logger, **kwargs)
        except:
            import pdb
            import traceback
            traceback.print_exc()
            pdb.post_mortem()

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

def create_parser_wpt():
    from wptrunner import wptcommandline
    return wptcommandline.create_parser(["firefox"])

def create_parser_update():
    from update import updatecommandline
    return updatecommandline.create_parser()

def create_parser_reduce():
    from wptrunner import wptcommandline
    return wptcommandline.create_parser_reduce()

@CommandProvider
class MachCommands(MachCommandBase):
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

        wpt_runner = self._spawn(WebPlatformTestsRunner)

        if params["list_test_groups"]:
            return wpt_runner.list_test_groups(**params)
        else:
            return wpt_runner.run_tests(**params)

    @Command("web-platform-tests-update",
             category="testing",
             parser=create_parser_update)
    def update_web_platform_tests(self, **params):
        self.setup()
        self.virtualenv_manager.install_pip_package('html5lib==0.99')
        self.virtualenv_manager.install_pip_package('requests')
        wpt_updater = self._spawn(WebPlatformTestsUpdater)
        return wpt_updater.run_update(**params)

    def setup(self):
        self._activate_virtualenv()

    @Command("web-platform-tests-reduce",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=create_parser_reduce)
    def unstable_web_platform_tests(self, **params):
        self.setup()
        wpt_reduce = self._spawn(WebPlatformTestsReduce)
        return wpt_reduce.run_reduce(**params)
