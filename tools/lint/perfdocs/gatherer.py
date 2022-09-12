# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import pathlib

from perfdocs.logger import PerfDocLogger
from perfdocs.utils import read_yaml
from perfdocs.framework_gatherers import (
    MozperftestGatherer,
    RaptorGatherer,
    StaticGatherer,
    TalosGatherer,
    AwsyGatherer,
)

logger = PerfDocLogger()

# TODO: Implement decorator/searcher to find the classes.
frameworks = {
    "raptor": RaptorGatherer,
    "mozperftest": MozperftestGatherer,
    "talos": TalosGatherer,
    "awsy": AwsyGatherer,
}


class Gatherer(object):
    """
    Gatherer produces the tree of the perfdoc's entries found
    and can obtain manifest-based test lists. Used by the Verifier.
    """

    def __init__(self, workspace_dir, taskgraph=None):
        """
        Initialzie the Gatherer.

        :param str workspace_dir: Path to the gecko checkout.
        """
        self.workspace_dir = workspace_dir
        self.taskgraph = taskgraph
        self._perfdocs_tree = []
        self._test_list = []
        self.framework_gatherers = {}

    @property
    def perfdocs_tree(self):
        """
        Returns the perfdocs_tree, and computes it
        if it doesn't exist.

        :return dict: The perfdocs tree containing all
            framework perfdoc entries. See `fetch_perfdocs_tree`
            for information on the data structure.
        """
        if self._perfdocs_tree:
            return self._perfdocs_tree
        else:
            self.fetch_perfdocs_tree()
            return self._perfdocs_tree

    def fetch_perfdocs_tree(self):
        """
        Creates the perfdocs tree with the following structure:
            [
                {
                    "path": Path to the perfdocs directory.
                    "yml": Name of the configuration YAML file.
                    "rst": Name of the RST file.
                    "static": Name of the static file.
                }, ...
            ]

        This method doesn't return anything. The result can be found in
        the perfdocs_tree attribute.
        """
        exclude_dir = [
            ".hg",
            str(pathlib.Path("tools", "lint")),
            str(pathlib.Path("testing", "perfdocs")),
        ]

        for path in pathlib.Path(self.workspace_dir).rglob("perfdocs"):
            if any(d in str(path.resolve()) for d in exclude_dir):
                continue
            files = [f for f in os.listdir(path)]
            matched = {"path": str(path), "yml": "", "rst": "", "static": []}

            for file in files:
                # Add the yml/rst/static file to its key if re finds the searched file
                if file == "config.yml" or file == "config.yaml":
                    matched["yml"] = file
                elif file == "index.rst":
                    matched["rst"] = file
                elif file.endswith(".rst"):
                    matched["static"].append(file)

            # Append to structdocs if all the searched files were found
            if all(val for val in matched.values() if not type(val) == list):
                self._perfdocs_tree.append(matched)

        logger.log(
            "Found {} perfdocs directories in {}".format(
                len(self._perfdocs_tree),
                [d["path"] for d in self._perfdocs_tree],
            )
        )

    def get_test_list(self, sdt_entry):
        """
        Use a perfdocs_tree entry to find the test list for
        the framework that was found.

        :return: A framework info dictionary with fields: {
            'yml_path': Path to YAML,
            'yml_content': Content of YAML,
            'name': Name of framework,
            'test_list': Test list found for the framework
        }
        """

        # If it was computed before, return it
        yaml_path = pathlib.Path(sdt_entry["path"], sdt_entry["yml"])
        for entry in self._test_list:
            if entry["yml_path"] == yaml_path:
                return entry

        # Set up framework entry with meta data
        yaml_content = read_yaml(yaml_path)
        framework = {
            "yml_content": yaml_content,
            "yml_path": yaml_path,
            "name": yaml_content["name"],
            "test_list": {},
        }

        if yaml_content["static-only"]:
            framework_gatherer_cls = StaticGatherer
        else:
            framework_gatherer_cls = frameworks[framework["name"]]

        # Get and then store the frameworks tests
        framework_gatherer = self.framework_gatherers[
            framework["name"]
        ] = framework_gatherer_cls(
            framework["yml_path"], self.workspace_dir, self.taskgraph
        )

        if not yaml_content["static-only"]:
            framework["test_list"] = framework_gatherer.get_test_list()

        self._test_list.append(framework)
        return framework
