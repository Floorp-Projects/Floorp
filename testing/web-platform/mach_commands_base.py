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

    def setup_logging(self, **kwargs):
        from wptrunner import wptrunner
        return wptrunner.setup_logging(kwargs, {self.setup.default_log_type: sys.stdout})

    def run(self, logger, **kwargs):
        from wptrunner import wptrunner

        if kwargs["manifest_update"] is not False:
            self.update_manifest(logger)
        kwargs["manifest_update"] = False

        if kwargs["product"] in ["firefox", None]:
            kwargs = self.setup.kwargs_firefox(kwargs)
        elif kwargs["product"] == "fennec":
            from wptrunner import wptcommandline
            kwargs = wptcommandline.check_args(self.setup.kwargs_common(kwargs))
        elif kwargs["product"] in ("chrome", "edge", "servo"):
            kwargs = self.setup.kwargs_wptrun(kwargs)
        else:
            raise ValueError("Unknown product %s" % kwargs["product"])
        result = wptrunner.start(**kwargs)
        return int(not result)

    def update_manifest(self, logger, **kwargs):
        import manifestupdate
        return manifestupdate.run(logger=logger,
                                  src_root=self.setup.topsrcdir,
                                  obj_root=self.setup.topobjdir,
                                  **kwargs)
