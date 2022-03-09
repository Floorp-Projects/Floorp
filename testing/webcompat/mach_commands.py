# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import sys

from mach.decorators import Command
from mozbuild.base import MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))


def create_parser_interventions():
    from mozlog import commandline

    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", help="Path to browser binary")
    parser.add_argument("--webdriver-binary", help="Path to webdriver binary")
    parser.add_argument("--bug", help="Bug to run tests for")
    parser.add_argument(
        "--config", help="Path to JSON file containing logins and other settings"
    )
    parser.add_argument(
        "--debug", action="store_true", default=False, help="Debug failing tests"
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        default=False,
        help="Run firefox in headless mode",
    )
    parser.add_argument(
        "--interventions",
        action="store",
        default="both",
        choices=["enabled", "disabled", "both"],
        help="Enable webcompat interventions",
    )
    commandline.add_logging_group(parser)
    return parser


class InterventionTest(MozbuildObject):
    def set_default_kwargs(self, kwargs):
        if kwargs["binary"] is None:
            kwargs["binary"] = self.get_binary_path()

        if kwargs["webdriver_binary"] is None:
            kwargs["webdriver_binary"] = self.get_binary_path(
                "geckodriver", validate_exists=False
            )

    def get_capabilities(self, kwargs):
        return {"moz:firefoxOptions": {"binary": kwargs["binary"]}}

    def run(self, **kwargs):
        import runner
        import mozlog

        mozlog.commandline.setup_logging(
            "test-interventions", kwargs, {"mach": sys.stdout}
        )
        logger = mozlog.get_default_logger("test-interventions")
        status_handler = mozlog.handlers.StatusHandler()
        logger.add_handler(status_handler)

        self.set_default_kwargs(kwargs)

        interventions = (
            ["enabled", "disabled"]
            if kwargs["interventions"] == "both"
            else [kwargs["interventions"]]
        )

        for interventions_setting in interventions:
            runner.run(
                logger,
                os.path.join(here, "interventions"),
                kwargs["binary"],
                kwargs["webdriver_binary"],
                bug=kwargs["bug"],
                debug=kwargs["debug"],
                interventions=interventions_setting,
                config=kwargs["config"],
                headless=kwargs["headless"],
            )

        summary = status_handler.summarize()
        passed = (
            summary.unexpected_statuses == 0
            and summary.log_level_counts.get("ERROR", 0) == 0
            and summary.log_level_counts.get("CRITICAL", 0) == 0
        )
        return passed


@Command(
    "test-interventions",
    category="testing",
    description="Test the webcompat interventions",
    parser=create_parser_interventions,
    virtualenv_name="webcompat",
)
def test_interventions(command_context, **params):
    command_context.activate_virtualenv()
    intervention_test = command_context._spawn(InterventionTest)
    return 0 if intervention_test.run(**params) else 1
