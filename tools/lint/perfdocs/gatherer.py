# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import re

from perfdocs.logger import PerfDocLogger
from perfdocs.utils import read_yaml
from perfdocs.framework_gatherers import RaptorGatherer

logger = PerfDocLogger()

# TODO: Implement decorator/searcher to find the classes.
frameworks = {
    "raptor": RaptorGatherer,
}


class Gatherer(object):
    '''
    Gatherer produces the tree of the perfdoc's entries found
    and can obtain manifest-based test lists. Used by the Verifier.
    '''

    def __init__(self, root_dir, workspace_dir):
        '''
        Initialzie the Gatherer.

        :param str root_dir: Path to the testing directory.
        :param str workspace_dir: Path to the gecko checkout.
        '''
        self.root_dir = root_dir
        self.workspace_dir = workspace_dir
        self._perfdocs_tree = []
        self._test_list = []

    @property
    def perfdocs_tree(self):
        '''
        Returns the perfdocs_tree, and computes it
        if it doesn't exist.

        :return dict: The perfdocs tree containing all
            framework perfdoc entries. See `fetch_perfdocs_tree`
            for information on the data strcture.
        '''
        if self._perfdocs_tree:
            return self.perfdocs_tree
        else:
            self.fetch_perfdocs_tree()
            return self._perfdocs_tree

    def fetch_perfdocs_tree(self):
        '''
        Creates the perfdocs tree with the following structure:
            [
                {
                    "path": Path to the perfdocs directory.
                    "yml": Name of the configuration YAML file.
                    "rst": Name of the RST file.
                }, ...
            ]

        This method doesn't return anything. The result can be found in
        the perfdocs_tree attribute.
        '''
        yml_match = re.compile('^config.y(a)?ml$')
        rst_match = re.compile('^index.rst$')

        for dirpath, dirname, files in os.walk(self.root_dir):
            # Walk through the testing directory tree
            if dirpath.endswith('/perfdocs'):
                matched = {"path": dirpath, "yml": "", "rst": ""}
                for file in files:
                    # Add the yml/rst file to its key if re finds the searched file
                    if re.search(yml_match, file):
                        matched["yml"] = re.search(yml_match, file).string
                    if re.search(rst_match, file):
                        matched["rst"] = re.search(rst_match, file).string
                    # Append to structdocs if all the searched files were found
                    if all(matched.values()):
                        self._perfdocs_tree.append(matched)

        logger.log("Found {} perfdocs directories in {}"
                   .format(len(self._perfdocs_tree), self.root_dir))

    def get_test_list(self, sdt_entry):
        '''
        Use a perfdocs_tree entry to find the test list for
        the framework that was found.

        :return: A framework info dictionary with fields: {
            'yml_path': Path to YAML,
            'yml_content': Content of YAML,
            'name': Name of framework,
            'test_list': Test list found for the framework
        }
        '''

        # If it was computed before, return it
        yaml_path = os.path.join(sdt_entry["path"], sdt_entry['yml'])
        for entry in self._test_list:
            if entry['yml_path'] == yaml_path:
                return entry

        # Set up framework entry with meta data
        yaml_content = read_yaml(yaml_path)
        framework = {
            'yml_content': yaml_content,
            'yml_path': yaml_path,
            'name': yaml_content["name"],
        }

        # Get and then store the frameworks tests
        framework_gatherer = frameworks[framework["name"]](
            framework["yml_path"],
            self.workspace_dir
        )
        framework["test_list"] = framework_gatherer.get_suite_list()

        self._test_list.append(framework)
        return framework
