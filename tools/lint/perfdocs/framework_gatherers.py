# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import re

from perfdocs.utils import read_yaml
from manifestparser import TestManifest

'''
This file is for framework specific gatherers since manifests
might be parsed differently in each of them. The gatherers
must implement the FrameworkGatherer class.
'''


class FrameworkGatherer(object):
    '''
    Abstract class for framework gatherers.
    '''

    def __init__(self, yaml_path, workspace_dir):
        '''
        Generic initialization for a framework gatherer.
        '''
        self.workspace_dir = workspace_dir
        self._yaml_path = yaml_path
        self._suite_list = {}
        self._manifest_path = ''
        self._manifest = None

    def get_manifest_path(self):
        '''
        Returns the path to the manifest based on the
        manifest entry in the frameworks YAML configuration
        file.

        :return str: Path to the manifest.
        '''
        if self._manifest_path:
            return self._manifest_path

        yaml_content = read_yaml(self._yaml_path)
        self._manifest_path = os.path.join(
            self.workspace_dir, yaml_content["manifest"]
        )
        return self._manifest_path

    def get_suite_list(self):
        '''
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
        '''
        raise NotImplementedError


class RaptorGatherer(FrameworkGatherer):
    '''
    Gatherer for the Raptor framework.
    '''

    def get_suite_list(self):
        '''
        Returns a dictionary containing a mapping from suites
        to the tests they contain.

        :return dict: A dictionary with the following structure: {
                "suite_name": [
                    'testing/raptor/test1',
                    'testing/raptor/test2'
                ]
            }
        '''
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
            fpath = re.sub(".*testing", "testing", test['manifest'])

            if fpath not in self._suite_list[s]:
                self._suite_list[s].append(fpath)

        return self._suite_list
