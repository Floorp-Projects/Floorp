# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

from mach_commands_base import WebPlatformTestsRunner, create_parser_wpt
from mach.decorators import (
    CommandProvider,
    Command,
)


class WebPlatformTestsRunnerSetup(object):
    default_log_type = "tbpl"

    def __init__(self, context):
        self.context = context

    def kwargs_firefox(self, kwargs):
        from wptrunner import wptcommandline
        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.context.package_root, 'web-platform', 'wptrunner.ini')
        if kwargs["binary"] is None:
            kwargs["binary"] = self.context.firefox_bin
        if kwargs["prefs_root"] is None:
            kwargs["prefs_root"] = os.path.join(self.context.package_root, 'web-platform', "prefs")
        if kwargs["certutil_binary"] is None:
            kwargs["certutil_binary"] = os.path.join(self.context.bin_dir, 'certutil')
        if kwargs["stackfix_dir"] is None:
            kwargs["stackfix_dir"] = self.context.bin_dir
        if kwargs["ssl_type"] in (None, "pregenerated"):
            if kwargs["ca_cert_path"] is None:
                kwargs["ca_cert_path"] = os.path.join(self.context.package_root, "web-platform", "certs", "cacert.pem")
            if kwargs["host_key_path"] is None:
                kwargs["host_key_path"] = os.path.join(self.context.package_root, "web-platform", "certs", "web-platform.test.key")
            if kwargs["host_cert_path"] is None:
                kwargs["host_cert_path"] = os.path.join(self.context.package_root, "web-platform", "certs", "web-platform.test.pem")
        kwargs["capture_stdio"] = True

        if kwargs["exclude"] is None and kwargs["include"] is None and not sys.platform.startswith("linux"):
            kwargs["exclude"] = ["css"]

        if kwargs["webdriver_binary"] is None:
            kwargs["webdriver_binary"] = os.path.join(self.context.bin_dir, "geckodriver")

        kwargs = wptcommandline.check_args(kwargs)

    def kwargs_wptrun(self, kwargs):
        raise NotImplementedError


@CommandProvider
class MachCommands(object):
    def __init__(self, context):
        self.context = context

    @Command("web-platform-tests",
             category="testing",
             parser=create_parser_wpt)
    def run_web_platform_tests(self, **kwargs):
        self.context.activate_mozharness_venv()
        return WebPlatformTestsRunner(WebPlatformTestsRunnerSetup(self.context)).run(**kwargs)

    @Command("wpt",
             category="testing",
             parser=create_parser_wpt)
    def run_wpt(self, **params):
        return self.run_web_platform_tests(**params)
