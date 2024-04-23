# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
from pathlib import Path
from typing import List, Union

from mozlint.parser import Parser
from mozlint.pathutils import filterpaths
from taskgraph.optimize.base import OptimizationStrategy
from taskgraph.util.path import match as match_path

from gecko_taskgraph import GECKO

logger = logging.getLogger(__name__)


class TGMozlintParser(Parser):
    """
    Mozlint Parser that skips validation. This is needed because decision
    tasks use sparse clones and the files themselves are not present.
    """

    def _validate(self, linter):
        pass


class SkipUnlessMozlint(OptimizationStrategy):
    """
    Optimization strategy for mozlint tests.

    Uses the mozlint YAML file for each test to determine whether or not a test
    should run.

    Using:
        - The optimization relies on having `files_changed` set in the decision task
          parameters.
        - Register with register_strategy. The argument is the path to MozLint YAML
          configuration files ("/tools/lint" for mozilla-central):
        - In source-test/mozlint.yml, set the optimization strategy for each job by filename:
        - For Mozlint jobs that run multiple linters at once use a list of filenames:
    """

    def __init__(self, linters_path: str):
        self.mozlint_parser = TGMozlintParser(GECKO)
        self.linters_path = Path(GECKO) / linters_path

    def should_remove_task(
        self, task, params, mozlint_confs: Union[str, List[str]]
    ) -> bool:
        include = []
        exclude = []
        extensions = []
        exclude_extensions = []
        support_files = ["python/mozlint/**", "tools/lint/**"]

        files_changed = params["files_changed"]

        if isinstance(mozlint_confs, str):
            mozlint_confs = [mozlint_confs]

        for mozlint_conf in mozlint_confs:
            mozlint_yaml = str(self.linters_path / mozlint_conf)
            logger.info(f"Loading file patterns for {task.label} from {mozlint_yaml}.")
            linter_config = self.mozlint_parser(mozlint_yaml)

            for config in linter_config:
                include += config.get("include", [])
                exclude += config.get("exclude", [])
                extensions += [e.strip(".") for e in config.get("extensions", [])]
                exclude_extensions += [
                    e.strip(".") for e in config.get("exclude_extensions", [])
                ]
                support_files += config.get("support-files", [])

        # Support files may not be part of "include" patterns, so check first
        # Do not remove (return False) if any changed
        for pattern in set(support_files):
            for path in files_changed:
                if match_path(path, pattern):
                    return False

        to_lint, to_exclude = filterpaths(
            GECKO,
            list(files_changed),
            include=include,
            exclude=exclude,
            extensions=extensions,
            exclude_extensions=exclude_extensions,
        )

        # to_lint should be an empty list if there is nothing to check
        if not to_lint:
            return True
        return False
