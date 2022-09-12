# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import jsonschema
import os
import pathlib
import re

from perfdocs.logger import PerfDocLogger
from perfdocs.utils import read_file, read_yaml
from perfdocs.gatherer import Gatherer

logger = PerfDocLogger()

"""
Schema for the config.yml file.
Expecting a YAML file with a format such as this:

name: raptor
manifest: testing/raptor/raptor/raptor.ini
static-only: False
suites:
    desktop:
        description: "Desktop tests."
        tests:
            raptor-tp6: "Raptor TP6 tests."
    mobile:
        description: "Mobile tests"
    benchmarks:
        description: "Benchmark tests."
        tests:
            wasm: "All wasm tests."

"""
CONFIG_SCHEMA = {
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "manifest": {"type": "string"},
        "static-only": {"type": "boolean"},
        "suites": {
            "type": "object",
            "properties": {
                "suite_name": {
                    "type": "object",
                    "properties": {
                        "tests": {
                            "type": "object",
                            "properties": {"test_name": {"type": "string"}},
                        },
                        "description": {"type": "string"},
                        "owner": {"type": "string"},
                    },
                    "required": ["description"],
                }
            },
        },
    },
    "required": ["name", "manifest", "static-only", "suites"],
}


class Verifier(object):
    """
    Verifier is used for validating the perfdocs folders/tree. In the future,
    the generator will make use of this class to obtain a validated set of
    descriptions that can be used to build up a document.
    """

    def __init__(self, workspace_dir, taskgraph=None):
        """
        Initialize the Verifier.

        :param str workspace_dir: Path to the top-level checkout directory.
        """
        self.workspace_dir = workspace_dir
        self._gatherer = Gatherer(workspace_dir, taskgraph)

    def validate_descriptions(self, framework_info):
        """
        Cross-validate the tests found in the manifests and the YAML
        test definitions. This function doesn't return a valid flag. Instead,
        the StructDocLogger.VALIDATION_LOG is used to determine validity.

        The validation proceeds as follows:
            1. Check that all tests/suites in the YAML exist in the manifests.
                - At the same time, build a list of global descriptions which
                   define descriptions for groupings of tests.
            2. Check that all tests/suites found in the manifests exist in the YAML.
                - For missing tests, check if a global description for them exists.

        As the validation is completed, errors are output into the validation log
        for any issues that are found.

        :param dict framework_info: Contains information about the framework. See
            `Gatherer.get_test_list` for information about its structure.
        """
        yaml_content = framework_info["yml_content"]

        # Check for any bad test/suite names in the yaml config file
        global_descriptions = {}
        for suite, ytests in yaml_content["suites"].items():
            # Find the suite, then check against the tests within it
            if framework_info["test_list"].get(suite):
                global_descriptions[suite] = []
                if not ytests.get("tests"):
                    # It's possible a suite entry has no tests
                    continue

                # Suite found - now check if any tests in YAML
                # definitions doesn't exist
                ytests = ytests["tests"]
                for test_name in ytests:
                    foundtest = False
                    for t in framework_info["test_list"][suite]:
                        tb = os.path.basename(str(t))
                        tb = re.sub("\..*", "", tb)
                        if test_name == tb:
                            # Found an exact match for the test_name
                            foundtest = True
                            break
                        if test_name in tb:
                            # Found a 'fuzzy' match for the test_name
                            # i.e. 'wasm' could exist for all raptor wasm tests
                            global_descriptions[suite].append(test_name)
                            foundtest = True
                            break
                    if not foundtest:
                        logger.warning(
                            "Could not find an existing test for {} - bad test name?".format(
                                test_name
                            ),
                            framework_info["yml_path"],
                        )
            else:
                logger.warning(
                    "Could not find an existing suite for {} - bad suite name?".format(
                        suite
                    ),
                    framework_info["yml_path"],
                )

        # Check for any missing tests/suites
        for suite, test_list in framework_info["test_list"].items():
            if not yaml_content["suites"].get(suite):
                # Description doesn't exist for the suite
                logger.warning(
                    "Missing suite description for {}".format(suite),
                    yaml_content["manifest"],
                )
                continue

            # If only a description is provided for the suite, assume
            # that this is a suite-wide description and don't check for
            # it's tests
            stests = yaml_content["suites"][suite].get("tests", None)
            if not stests:
                continue

            tests_found = 0
            missing_tests = []
            test_to_manifest = {}
            for test_name, manifest_path in test_list.items():
                tb = os.path.basename(str(manifest_path))
                tb = re.sub("\..*", "", tb)
                if (
                    stests.get(tb, None) is not None
                    or stests.get(test_name, None) is not None
                ):
                    # Test description exists, continue with the next test
                    tests_found += 1
                    continue
                test_to_manifest[test_name] = manifest_path
                missing_tests.append(test_name)

            # Check if global test descriptions exist (i.e.
            # ones that cover all of tp6) for the missing tests
            new_mtests = []
            for mt in missing_tests:
                found = False
                for test_name in global_descriptions[suite]:
                    # Global test exists for this missing test
                    if mt.startswith(test_name):
                        found = True
                        break
                    if test_name in mt:
                        found = True
                        break
                if not found:
                    new_mtests.append(mt)

            if len(new_mtests):
                # Output an error for each manifest with a missing
                # test description
                for test_name in new_mtests:
                    logger.warning(
                        "Could not find a test description for {}".format(test_name),
                        test_to_manifest[test_name],
                    )

    def validate_yaml(self, yaml_path):
        """
        Validate that the YAML file has all the fields that are
        required and parse the descriptions into strings in case
        some are give as relative file paths.

        :param str yaml_path: Path to the YAML to validate.
        :return bool: True/False => Passed/Failed Validation
        """

        def _get_description(desc):
            """
            Recompute the description in case it's a file.
            """
            desc_path = pathlib.Path(self.workspace_dir, desc)

            try:
                if desc_path.exists() and desc_path.is_file():
                    with open(desc_path, "r") as f:
                        desc = f.readlines()
            except OSError:
                pass

            return desc

        def _parse_descriptions(content):
            for suite, sinfo in content.items():
                desc = sinfo["description"]
                sinfo["description"] = _get_description(desc)

                # It's possible that the suite has no tests and
                # only a description. If they exist, then parse them.
                if "tests" in sinfo:
                    for test, desc in sinfo["tests"].items():
                        sinfo["tests"][test] = _get_description(desc)

        valid = False
        yaml_content = read_yaml(yaml_path)

        try:
            jsonschema.validate(instance=yaml_content, schema=CONFIG_SCHEMA)
            _parse_descriptions(yaml_content["suites"])
            valid = True
        except Exception as e:
            logger.warning("YAML ValidationError: {}".format(str(e)), yaml_path)

        return valid

    def validate_rst_content(self, rst_path):
        """
        Validate that the index file given has a {documentation} entry
        so that the documentation can be inserted there.

        :param str rst_path: Path to the RST file.
        :return bool: True/False => Passed/Failed Validation
        """
        rst_content = read_file(rst_path)

        # Check for a {documentation} entry in some line,
        # if we can't find one, then the validation fails.
        valid = False
        docs_match = re.compile(".*{documentation}.*")
        for line in rst_content:
            if docs_match.search(line):
                valid = True
                break
        if not valid:
            logger.warning(
                "Cannot find a '{documentation}' entry in the given index file",
                rst_path,
            )

        return valid

    def _check_framework_descriptions(self, item):
        """
        Helper method for validating descriptions
        """
        framework_info = self._gatherer.get_test_list(item)
        self.validate_descriptions(framework_info)

    def validate_tree(self):
        """
        Validate the `perfdocs` directory that was found.
        Returns True if it is good, false otherwise.

        :return bool: True/False => Passed/Failed Validation
        """
        found_good = 0

        # For each framework, check their files and validate descriptions
        for matched in self._gatherer.perfdocs_tree:
            # Get the paths to the YAML and RST for this framework
            matched_yml = pathlib.Path(matched["path"], matched["yml"])
            matched_rst = pathlib.Path(matched["path"], matched["rst"])

            _valid_files = {
                "yml": self.validate_yaml(matched_yml),
                "rst": True,
            }
            if not read_yaml(matched_yml)["static-only"]:
                _valid_files["rst"] = self.validate_rst_content(matched_rst)

            # Log independently the errors found for the matched files
            for file_format, valid in _valid_files.items():
                if not valid:
                    logger.log("File validation error: {}".format(file_format))
            if not all(_valid_files.values()):
                continue
            found_good += 1

            self._check_framework_descriptions(matched)

        if not found_good:
            raise Exception("No valid perfdocs directories found")
