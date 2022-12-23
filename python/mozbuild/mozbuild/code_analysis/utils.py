# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging

import mozpack.path as mozpath
import yaml

from mozbuild.util import memoized_property


class ClangTidyConfig(object):
    def __init__(self, mozilla_src):
        self._clang_tidy_config = self._get_clang_tidy_config(mozilla_src)

    def _get_clang_tidy_config(self, mozilla_src):
        try:
            file_handler = open(
                mozpath.join(mozilla_src, "tools", "clang-tidy", "config.yaml")
            )
            config = yaml.safe_load(file_handler)
        except Exception:
            self.log(
                logging.ERROR,
                "clang-tidy-config",
                {},
                "Looks like config.yaml is not valid, we are going to use default"
                " values for the rest of the analysis for clang-tidy.",
            )
            return None
        return config

    @memoized_property
    def checks(self):
        """
        Returns a list with all activated checks
        """

        checks = ["-*"]
        try:
            config = self._clang_tidy_config
            for item in config["clang_checkers"]:
                if item.get("publish", True):
                    checks.append(item["name"])
        except Exception:
            self.log(
                logging.ERROR,
                "clang-tidy-config",
                {},
                "Looks like config.yaml is not valid, so we are unable to "
                "determine default checkers, using '-checks=-*,mozilla-*'",
            )
            checks.append("mozilla-*")
        finally:
            return checks

    @memoized_property
    def checks_with_data(self):
        """
        Returns a list with all activated checks plus metadata for each check
        """

        checks_with_data = [{"name": "-*"}]
        try:
            config = self._clang_tidy_config
            for item in config["clang_checkers"]:
                if item.get("publish", True):
                    checks_with_data.append(item)
        except Exception:
            self.log(
                logging.ERROR,
                "clang-tidy-config",
                {},
                "Looks like config.yaml is not valid, so we are unable to "
                "determine default checkers, using '-checks=-*,mozilla-*'",
            )
            checks_with_data.append({"name": "mozilla-*", "reliability": "high"})
        finally:
            return checks_with_data

    @memoized_property
    def checks_config(self):
        """
        Returns the configuation for all checks
        """

        config_list = []
        checks_config = {}
        try:
            config = self._clang_tidy_config
            for checker in config["clang_checkers"]:
                if checker.get("publish", True) and "config" in checker:
                    for checker_option in checker["config"]:
                        # Verify if the format of the Option is correct,
                        # possibilities are:
                        # 1. CheckerName.Option
                        # 2. Option -> that will become CheckerName.Option
                        if not checker_option["key"].startswith(checker["name"]):
                            checker_option["key"] = "{}.{}".format(
                                checker["name"], checker_option["key"]
                            )
                    config_list += checker["config"]
            checks_config["CheckOptions"] = config_list
        except Exception:
            self.log(
                logging.ERROR,
                "clang-tidy-config",
                {},
                "Looks like config.yaml is not valid, so we are unable to "
                "determine configuration for checkers, so using default",
            )
            checks_config = None
        finally:
            return checks_config

    @memoized_property
    def version(self):
        """
        Returns version of clang-tidy suitable for this configuration file
        """

        if "package_version" in self._clang_tidy_config:
            return self._clang_tidy_config["package_version"]
        self.log(
            logging.ERROR,
            "clang-tidy-confis",
            {},
            "Unable to find 'package_version' in the config.yml",
        )
        return None

    @memoized_property
    def platforms(self):
        """
        Returns a list of platforms suitable to work with `clang-tidy`
        """
        return self._clang_tidy_config.get("platforms", [])
