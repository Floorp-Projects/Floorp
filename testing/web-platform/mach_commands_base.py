# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys


def create_parser_wpt():
    from wptrunner import wptcommandline
    return wptcommandline.create_parser(["fennec", "firefox", "chrome", "edge", "servo"])


class WebPlatformTestsRunner(object):
    """Run web platform tests."""

    def __init__(self, setup):
        self.setup = setup

    def run(self, **kwargs):
        from wptrunner import wptrunner
        if kwargs["product"] in ["firefox", None]:
            kwargs = self.setup.kwargs_firefox(kwargs)
        elif kwargs["product"] == "fennec":
            from wptrunner import wptcommandline
            kwargs = wptcommandline.check_args(self.setup.kwargs_common(kwargs))
        elif kwargs["product"] in ("chrome", "edge", "servo"):
            kwargs = self.setup.kwargs_wptrun(kwargs)
        else:
            raise ValueError("Unknown product %s" % kwargs["product"])
        logger = wptrunner.setup_logging(kwargs, {self.setup.default_log_type: sys.stdout})
        result = wptrunner.start(**kwargs)
        return int(not result)
