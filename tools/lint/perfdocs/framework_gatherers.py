# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import pathlib
import re

from manifestparser import TestManifest
from mozperftest.script import ScriptInfo
from perfdocs.utils import read_yaml
from perfdocs.logger import PerfDocLogger

logger = PerfDocLogger()

"""
This file is for framework specific gatherers since manifests
might be parsed differently in each of them. The gatherers
must implement the FrameworkGatherer class.
"""


class FrameworkGatherer(object):
    """
    Abstract class for framework gatherers.
    """

    def __init__(self, yaml_path, workspace_dir, taskgraph={}):
        """
        Generic initialization for a framework gatherer.
        """
        self.workspace_dir = workspace_dir
        self._yaml_path = yaml_path
        self._taskgraph = taskgraph
        self._suite_list = {}
        self._test_list = {}
        self._descriptions = {}
        self._manifest_path = ""
        self._manifest = None
        self.script_infos = {}
        self._task_list = {}
        self._task_match_pattern = re.compile(r"([\w\W]*/[pgo|opt]*)-([\w\W]*)")

    def get_task_match(self, task_name):
        return re.search(self._task_match_pattern, task_name)

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
        :param header_type: type of the title heading
        """
        heading_map = {"H3": "=", "H4": "-", "H5": "^"}
        return [title, heading_map.get(header_type, "^") * len(title), content, ""]


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

    def _get_ci_tasks(self):
        for task in self._taskgraph.keys():
            if type(self._taskgraph[task]) == dict:
                command = self._taskgraph[task]["task"]["payload"].get("command", [])
                run_on_projects = self._taskgraph[task]["attributes"]["run_on_projects"]
            else:
                command = self._taskgraph[task].task["payload"].get("command", [])
                run_on_projects = self._taskgraph[task].attributes["run_on_projects"]

            test_match = re.search(r"[\s']--test[\s=](.+?)[\s']", str(command))
            task_match = self.get_task_match(task)
            if test_match and task_match:
                test = test_match.group(1)
                platform = task_match.group(1)
                test_name = task_match.group(2)

                item = {"test_name": test_name, "run_on_projects": run_on_projects}
                self._task_list.setdefault(test, {}).setdefault(platform, []).append(
                    item
                )

    def _get_subtests_from_ini(self, manifest_path, suite_name):
        """
        Returns a list of (sub)tests from an ini file containing the test definitions.

        :param str manifest_path: path to the ini file
        :return list: the list of the tests
        """
        desc_exclusion = ["here", "manifest", "manifest_relpath", "path", "relpath"]
        test_manifest = TestManifest([manifest_path], strict=False)
        test_list = test_manifest.active_tests(exists=False, disabled=False)
        subtests = {}
        for subtest in test_list:
            subtests[subtest["name"]] = subtest["manifest"]

            description = {}
            for key, value in subtest.items():
                if key not in desc_exclusion:
                    description[key] = value
            self._descriptions.setdefault(suite_name, []).append(description)

        self._descriptions[suite_name].sort(key=lambda item: item["name"])

        return subtests

    def get_test_list(self):
        """
        Returns a dictionary containing the tests in every suite ini file.

        :return dict: A dictionary with the following structure: {
                "suite_name": {
                    'raptor_test1',
                    'raptor_test2'
                },
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
            for manifest_path in manifest_paths:
                subtest_list = self._get_subtests_from_ini(manifest_path, suite_name)
                self._test_list[suite_name].update(subtest_list)

        self._get_ci_tasks()

        return self._test_list

    def build_test_description(self, title, test_description="", suite_name=""):
        matcher = []
        browsers = [
            "firefox",
            "chrome",
            "chromium",
            "refbrow",
            "fennec68",
            "geckoview",
            "fenix",
        ]
        test_name = [f"{title}-{browser}" for browser in browsers]
        test_name.append(title)

        for suite, val in self._descriptions.items():
            for test in val:
                if test["name"] in test_name and suite_name == suite:
                    matcher.append(test)

        if len(matcher) == 0:
            logger.critical(
                "No tests exist for the following name "
                "(obtained from config.yml): {}".format(title)
            )
            raise Exception(
                "No tests exist for the following name "
                "(obtained from config.yml): {}".format(title)
            )

        result = f".. dropdown:: {title} ({test_description})\n"
        result += f"   :container: + anchor-id-{title}-{suite_name[0]}\n\n"

        for idx, description in enumerate(matcher):
            if description["name"] != title:
                result += f"   {idx+1}. **{description['name']}**\n\n"
            if "owner" in description.keys():
                result += f"   **Owner**: {description['owner']}\n\n"

            for key in sorted(description.keys()):
                if key in ["owner", "name"]:
                    continue
                sub_title = key.replace("_", " ")
                if key == "test_url":
                    if "<" in description[key] or ">" in description[key]:
                        description[key] = description[key].replace("<", "\<")
                        description[key] = description[key].replace(">", "\>")
                    result += f"   * **{sub_title}**: `<{description[key]}>`__\n"
                elif key == "secondary_url":
                    result += f"   * **{sub_title}**: `<{description[key]}>`__\n"
                elif key in ["playback_pageset_manifest"]:
                    result += (
                        f"   * **{sub_title}**: "
                        f"{description[key].replace('{subtest}', description['name'])}\n"
                    )
                else:
                    if "\n" in description[key]:
                        description[key] = description[key].replace("\n", " ")
                    result += f"   * **{sub_title}**: {description[key]}\n"

            if self._task_list.get(title, []):
                result += "   * **Test Task**:\n"
                for platform in sorted(self._task_list[title]):
                    if suite_name == "mobile" and "android" in platform:
                        result += f"      * {platform}\n"
                    elif suite_name != "mobile" and "android" not in platform:
                        result += f"      * {platform}\n"

                    self._task_list[title][platform].sort(key=lambda x: x["test_name"])
                    for task in self._task_list[title][platform]:
                        run_on_project = ": " + (
                            ", ".join(task["run_on_projects"])
                            if task["run_on_projects"]
                            else "None"
                        )
                        if suite_name == "mobile" and "android" in platform:
                            result += (
                                f"            * {task['test_name']}{run_on_project}\n"
                            )
                        elif suite_name != "mobile" and "android" not in platform:
                            result += (
                                f"            * {task['test_name']}{run_on_project}\n"
                            )

            result += "\n"

        return [result]

    def build_suite_section(self, title, content):
        return self._build_section_with_header(
            title.capitalize(), content, header_type="H4"
        )


class MozperftestGatherer(FrameworkGatherer):
    """
    Gatherer for the Mozperftest framework.
    """

    def get_test_list(self):
        """
        Returns a dictionary containing the tests that are in perftest.ini manifest.

        :return dict: A dictionary with the following structure: {
                "suite_name": {
                    'perftest_test1',
                    'perftest_test2',
                },
            }
        """
        for path in pathlib.Path(self.workspace_dir).rglob("perftest.ini"):
            if "obj-" in str(path):
                continue
            suite_name = re.sub(self.workspace_dir, "", os.path.dirname(path))

            # If the workspace dir doesn't end with a forward-slash,
            # the substitution above won't work completely
            if suite_name.startswith("/") or suite_name.startswith("\\"):
                suite_name = suite_name[1:]

            # We have to add new paths to the logger as we search
            # because mozperftest tests exist in multiple places in-tree
            PerfDocLogger.PATHS.append(suite_name)

            # Get the tests from perftest.ini
            test_manifest = TestManifest([str(path)], strict=False)
            test_list = test_manifest.active_tests(exists=False, disabled=False)
            for test in test_list:
                si = ScriptInfo(test["path"])
                self.script_infos[si["name"]] = si
                self._test_list.setdefault(suite_name.replace("\\", "/"), {}).update(
                    {si["name"]: str(path)}
                )

        return self._test_list

    def build_test_description(self, title, test_description="", suite_name=""):
        return [str(self.script_infos[title])]

    def build_suite_section(self, title, content):
        return self._build_section_with_header(title, content, header_type="H4")


class TalosGatherer(FrameworkGatherer):
    def get_test_list(self):
        from talos import test as talos_test

        test_lists = talos_test.test_dict()
        mod = __import__("talos.test", fromlist=test_lists)

        suite_name = "Talos Tests"

        for test in test_lists:
            self._test_list.setdefault(suite_name, {}).update({test: ""})

            klass = getattr(mod, test)
            self._descriptions.setdefault(test, klass.__dict__)

        return self._test_list

    def build_test_description(self, title, test_description="", suite_name=""):
        result = f".. dropdown:: {title}\n"
        result += f"   :container: + anchor-id-{title}-{suite_name.split()[0]}\n\n"

        for key in sorted(self._descriptions[title]):
            if key.startswith("__") and key.endswith("__"):
                continue
            elif key == "filters":
                continue

            result += f"   * **{key}**: {self._descriptions[title][key]}\n"

        return [result]

    def build_suite_section(self, title, content):
        return self._build_section_with_header(title, content, header_type="H3")


class AwsyGatherer(FrameworkGatherer):
    """
    Gatherer for the Awsy framework.
    """

    def _generate_ci_tasks(self):
        for task_name in self._taskgraph.keys():
            task = self._taskgraph[task_name]

            if type(task) == dict:
                awsy_test = task["task"]["extra"].get("suite", [])
                run_on_projects = task["attributes"]["run_on_projects"]
            else:
                awsy_test = task.task["extra"].get("suite", [])
                run_on_projects = task.attributes["run_on_projects"]

            task_match = self.get_task_match(task_name)

            if "awsy" in awsy_test and task_match:
                platform = task_match.group(1)
                test_name = task_match.group(2)
                item = {"test_name": test_name, "run_on_projects": run_on_projects}
                self._task_list.setdefault(platform, []).append(item)

    def get_suite_list(self):
        self._suite_list = {"Awsy tests": ["tp6", "base", "dmd", "tp5"]}
        return self._suite_list

    def get_test_list(self):
        self._generate_ci_tasks()
        return {
            "Awsy tests": {
                "tp6": "",
                "base": "",
                "dmd": "",
                "tp5": "",
            }
        }

    def build_suite_section(self, title, content):
        return self._build_section_with_header(
            title.capitalize(), content, header_type="H4"
        )

    def build_test_description(self, title, test_description="", suite_name=""):
        dropdown_suite_name = suite_name.replace(" ", "-")
        result = f".. dropdown:: {title} ({test_description})\n"
        result += f"   :container: + anchor-id-{title}-{dropdown_suite_name}\n\n"
        result += "   * **Test Task**:\n"

        # tp5 tests are represented by awsy-e10s test names
        # while the others have their title in test names
        search_tag = "awsy-e10s" if title == "tp5" else title
        for platform in sorted(self._task_list.keys()):
            result += f"      * {platform}\n"
            for test_dict in sorted(
                self._task_list[platform], key=lambda d: d["test_name"]
            ):
                if search_tag in test_dict["test_name"]:
                    run_on_project = ": " + (
                        ", ".join(test_dict["run_on_projects"])
                        if test_dict["run_on_projects"]
                        else "None"
                    )
                    result += (
                        f"            * {test_dict['test_name']}{run_on_project}\n"
                    )
            result += "\n"

        return [result]


class StaticGatherer(FrameworkGatherer):
    """
    A noop gatherer for frameworks with static-only documentation.
    """

    pass
