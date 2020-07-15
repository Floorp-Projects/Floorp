# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import re

from perfdocs.utils import read_yaml
from manifestparser import TestManifest

"""
This file is for framework specific gatherers since manifests
might be parsed differently in each of them. The gatherers
must implement the FrameworkGatherer class.
"""


class FrameworkGatherer(object):
    """
    Abstract class for framework gatherers.
    """

    def __init__(self, yaml_path, workspace_dir):
        """
        Generic initialization for a framework gatherer.
        """
        self.workspace_dir = workspace_dir
        self._yaml_path = yaml_path
        self._suite_list = {}
        self._test_list = {}
        self._manifest_path = ""
        self._manifest = None

    def get_manifest_path(self):
        """
        Returns the path to the manifest based on the
        manifest entry in the frameworks YAML configuration
        file.

        :return str: Path to the manifest.
        """
        if self._manifest_path:
            return self._manifest_path

        yaml_content = read_yaml(self._yaml_path)
        self._manifest_path = os.path.join(self.workspace_dir, yaml_content["manifest"])
        return self._manifest_path

    def get_suite_list(self):
        """
        Each framework gatherer must return a dictionary with
        the following structure. Note that the test names must
        be relative paths so that issues can be correctly issued
        by the reviewbot.

        :return dict: A dictionary with the following structure: {
                "suite_name": [
                    'testing/raptor/test1',
                    'testing/raptor/test2'
                ]
            }
        """
        raise NotImplementedError

    def _build_section_with_header(self, title, content, header_type=None):
        """
        Adds a section to the documentation with the title as the type mentioned
        and paragraph as content mentioned.
        :param title: title of the section
        :param content: content of section paragraph
        :param documentation: documentation object to add section to
        :param type: type of the title heading
        """
        heading_map = {"H4": "-", "H5": "^"}
        return [title, heading_map.get(type, "^") * len(title), content, ""]


class RaptorGatherer(FrameworkGatherer):
    """
    Gatherer for the Raptor framework.
    """

    def get_suite_list(self):
        """
        Returns a dictionary containing a mapping from suites
        to the tests they contain.

        :return dict: A dictionary with the following structure: {
                "suite_name": [
                    'testing/raptor/test1',
                    'testing/raptor/test2'
                ]
            }
        """
        if self._suite_list:
            return self._suite_list

        manifest_path = self.get_manifest_path()

        # Get the tests from the manifest
        test_manifest = TestManifest([manifest_path], strict=False)
        test_list = test_manifest.active_tests(exists=False, disabled=False)

        # Parse the tests into the expected dictionary
        for test in test_list:
            # Get the top-level suite
            s = os.path.basename(test["here"])
            if s not in self._suite_list:
                self._suite_list[s] = []

            # Get the individual test
            fpath = re.sub(".*testing", "testing", test["manifest"])

            if fpath not in self._suite_list[s]:
                self._suite_list[s].append(fpath)

        return self._suite_list

    def _get_subtests_from_ini(self, manifest_path):
        """
        Returns a list of (sub)tests from an ini file containing the test definitions.

        :param str manifest_path: path to the ini file
        :return list: the list of the tests
        """
        test_manifest = TestManifest([manifest_path], strict=False)
        test_list = test_manifest.active_tests(exists=False, disabled=False)
        subtest_list = {subtest["name"]: subtest["manifest"] for subtest in test_list}

        return subtest_list

    def get_test_list(self):
        """
        Returns a dictionary containing the tests in every suite ini file.

        :return dict: A dictionary with the following structure: {
                "suite_name": [
                    'raptor_test1',
                    'raptor_test2'
                ]
            }
        """
        if self._test_list:
            return self._test_list

        suite_list = self.get_suite_list()

        # Iterate over each manifest path from suite_list[suite_name]
        # and place the subtests into self._test_list under the same key
        for suite_name, manifest_paths in suite_list.items():
            if not self._test_list.get(suite_name):
                self._test_list[suite_name] = {}
            for i, manifest_path in enumerate(manifest_paths, 1):
                subtest_list = self._get_subtests_from_ini(manifest_path)
                self._test_list[suite_name].update(subtest_list)

        return self._test_list

    def build_test_description(self, title, test_description=""):
        return ["* " + title + " (" + test_description + ")"]


class MozperftestGatherer(FrameworkGatherer):
    """
    Gatherer for the Mozperftest framework.
    """

    pass
