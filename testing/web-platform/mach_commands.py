# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the web-platform-tests test runner with mach.

from __future__ import unicode_literals, print_function

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

from wptrunner import wptcommandline

# This should probably be consolidated with similar classes in other test
# runners.
class InvalidTestPathError(Exception):
    """Exception raised when the test path is not valid."""

class WebPlatformTestsRunner(MozbuildObject):
    """Run web platform tests."""

    def setup_kwargs(self, kwargs):
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topsrcdir, 'testing', 'web-platform', 'wptrunner.ini')

        if kwargs["binary"] is None:
            kwargs["binary"] = os.path.join(self.get_binary_path('app'))

        if kwargs["prefs_root"] is None:
            kwargs["prefs_root"] = os.path.join(self.topobjdir, '_tests', 'web-platform', "prefs")

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
        from wptrunner import update

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topsrcdir, 'testing', 'web-platform', 'wptrunner.ini')

        wptcommandline.set_from_config(kwargs)

        update.run_update(**kwargs)

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

@CommandProvider
class MachCommands(MachCommandBase):
    @Command("web-platform-tests",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=wptcommandline.create_parser(["firefox"]))
    def run_web_platform_tests(self, **params):
        self.setup()
        wpt_runner = self._spawn(WebPlatformTestsRunner)

        if params["list_test_groups"]:
            return wpt_runner.list_test_groups(**params)
        else:
            return wpt_runner.run_tests(**params)

    @Command("web-platform-tests-update",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=wptcommandline.create_parser_update())
    def update_web_platform_tests(self, **params):
        self.setup()
        self.virtualenv_manager.install_pip_package('html5lib==0.99')
        wpt_updater = self._spawn(WebPlatformTestsUpdater)
        return wpt_updater.run_update(**params)

    def setup(self):
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('py==1.4.14')

    @Command("web-platform-tests-reduce",
             category="testing",
             conditions=[conditions.is_firefox],
             parser=wptcommandline.create_parser_reduce(["firefox"]))
    def unstable_web_platform_tests(self, **params):
        self.setup()
        wpt_reduce = self._spawn(WebPlatformTestsReduce)
        return wpt_reduce.run_reduce(**params)
