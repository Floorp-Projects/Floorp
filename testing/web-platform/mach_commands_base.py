# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys


def create_parser_wpt():
    from wptrunner import wptcommandline
    result = wptcommandline.create_parser(["firefox", "firefox_android", "chrome", "edge", "servo"])
    result.add_argument("--no-install", action="store_true", default=False,
                        help="Do not install test runner application")
    return result


class WebPlatformTestsRunner(object):
    """Run web platform tests."""

    def __init__(self, setup):
        self.setup = setup

    def setup_logging(self, **kwargs):
        from tools.wpt import run
        return run.setup_logging(kwargs, {self.setup.default_log_type: sys.stdout},
                                 formatter_defaults={"screenshot": True})

    def run(self, logger, **kwargs):
        from wptrunner import wptrunner
        from mozbuild.base import BinaryNotFoundException

        if kwargs["manifest_update"] is not False:
            self.update_manifest(logger)
        kwargs["manifest_update"] = False

        if kwargs["product"] in ["firefox", None]:
            try:
                kwargs = self.setup.kwargs_firefox(kwargs)
            except BinaryNotFoundException as e:
                logger.error(e)
                logger.info(e.help())
                return 1
        elif kwargs["product"] == "firefox_android":
            from wptrunner import wptcommandline
            kwargs = wptcommandline.check_args(self.setup.kwargs_common(kwargs))
        elif kwargs["product"] in ("chrome", "edge", "servo"):
            kwargs = self.setup.kwargs_wptrun(kwargs)
        else:
            raise ValueError("Unknown product %s" % kwargs["product"])
        result = wptrunner.start(**kwargs)
        return int(result)

    def update_manifest(self, logger, **kwargs):
        import manifestupdate
        return manifestupdate.run(logger=logger,
                                  src_root=self.setup.topsrcdir,
                                  obj_root=self.setup.topobjdir,
                                  **kwargs)
