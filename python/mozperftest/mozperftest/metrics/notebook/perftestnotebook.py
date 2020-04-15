#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import pathlib
import yaml
from collections import OrderedDict

from .transformer import SimplePerfherderTransformer
from .analyzer import NotebookAnalyzer
from .constant import Constant
from .logger import NotebookLogger
from .notebookparser import parse_args

logger = NotebookLogger()


class PerftestNotebook(object):
    """
    Controller class for the Perftest-Notebook.
    """

    def __init__(self, file_groups, config, custom_transform=None, sort_files=False):
        """Initializes PerftestNotebook.

        :param dict file_groups: A dict of file groupings. The value
            of each of the dict entries is the name of the data that
            will be produced.
        :param str custom_transform: Path to a file containing custom
            transformation logic. Must implement the Transformer
            interface.
        """
        self.fmt_data = {}
        self.file_groups = file_groups
        self.config = config
        self.sort_files = sort_files
        self.const = Constant()

        # Gather the available transformers
        tfms_dict = self.const.predefined_transformers

        # XXX NOTEBOOK_PLUGIN functionality is broken at the moment.
        # This code block will raise an exception if it detects it in
        # the environment.
        plugin_path = os.getenv("NOTEBOOK_PLUGIN")
        if plugin_path:
            raise Exception("NOTEBOOK_PLUGIN is currently broken.")

        # Initialize the requested transformer
        if custom_transform:
            tfm_cls = tfms_dict.get(custom_transform)
            if tfm_cls:
                self.transformer = tfm_cls(files=[])
                logger.info(f"Found {custom_transform} transformer")
            else:
                raise Exception(f"Could not get a {custom_transform} transformer.")
        else:
            self.transformer = SimplePerfherderTransformer(files=[])

        self.analyzer = NotebookAnalyzer(data=None)

    def parse_file_grouping(self, file_grouping):
        """Handles differences in the file_grouping definitions.

        It can either be a path to a folder containing the files, a list of files,
        or it can contain settings from an artifact_downloader instance.

        :param file_grouping: A file grouping entry.
        :return: A list of files to process.
        """
        files = []
        if isinstance(file_grouping, list):
            # A list of files was provided
            files = file_grouping
        elif isinstance(file_grouping, dict):
            # A dictionary of settings from an artifact_downloader instance
            # was provided here
            print("awljdlkwad")
            raise Exception(
                "Artifact downloader tooling is disabled for the time being."
            )
        elif isinstance(file_grouping, str):
            # Assume a path to files was given
            filepath = files

            newf = [f for f in pathlib.Path(filepath).rglob("*.json")]
            if not newf:
                # Couldn't find any JSON files, so take all the files
                # in the directory
                newf = [f for f in pathlib.Path(filepath).rglob("*")]

            files = newf
        else:
            raise Exception(
                "Unknown file grouping type provided here: %s" % file_grouping
            )

        if self.sort_files:
            if isinstance(files, list):
                files.sort()
            else:
                for _, file_list in files.items():
                    file_list.sort()
                files = OrderedDict(sorted(files.items(), key=lambda entry: entry[0]))

        if not files:
            raise Exception(
                "Could not find any files in this configuration: %s" % file_grouping
            )

        return files

    def parse_output(self):
        # XXX Fix up this function, it should only return a directory for output
        # not a directory or a file. Or remove it completely, it's not very useful.
        prefix = "" if "prefix" not in self.config else self.config["prefix"]
        filepath = f"{prefix}std-output.json"

        if "output" in self.config:
            filepath = self.config["output"]
        if os.path.isdir(filepath):
            filepath = os.path.join(filepath, f"{prefix}std-output.json")

        return filepath

    def process(self, no_iodide=True):
        """Process the file groups and return the results of the requested analyses.

        :return: All the results in a dictionary. The field names are the Analyzer
            funtions that were called.
        """
        fmt_data = []

        for name, files in self.file_groups.items():
            files = self.parse_file_grouping(files)
            if isinstance(files, dict):
                for subtest, files in files.items():
                    self.transformer.files = files

                    trfm_data = self.transformer.process(name)

                    if isinstance(trfm_data, list):
                        for e in trfm_data:
                            if "subtest" not in e:
                                e["subtest"] = subtest
                            else:
                                e["subtest"] = "%s-%s" % (subtest, e["subtest"])
                        fmt_data.extend(trfm_data)
                    else:
                        if "subtest" not in trfm_data:
                            trfm_data["subtest"] = subtest
                        else:
                            trfm_data["subtest"] = "%s-%s" % (
                                subtest,
                                trfm_data["subtest"],
                            )
                        fmt_data.append(trfm_data)
            else:
                # Transform the data
                self.transformer.files = files
                trfm_data = self.transformer.process(name)

                if isinstance(trfm_data, list):
                    fmt_data.extend(trfm_data)
                else:
                    fmt_data.append(trfm_data)

        self.fmt_data = fmt_data

        # Write formatted data output to filepath
        output_data_filepath = self.parse_output()

        print("Writing results to %s" % output_data_filepath)

        with open(output_data_filepath, "w") as f:
            json.dump(self.fmt_data, f, indent=4, sort_keys=True)

        # Gather config["analysis"] corresponding notebook sections
        if "analysis" in self.config:
            raise Exception(
                "Analysis aspect of the notebook is disabled for the time being"
            )

        # Post to Iodide server
        if not no_iodide:
            raise Exception(
                "Opening report through Iodide is not available in production at the moment"
            )

        return {"data": self.fmt_data, "file-output": output_data_filepath}


def main():
    args = parse_args()

    NotebookLogger.debug = args.debug

    config = None
    with open(args.config, "r") as f:
        logger.info("yaml_path: {}".format(args.config))
        config = yaml.safe_load(f)

    custom_transform = config.get("custom_transform", None)

    ptnb = PerftestNotebook(
        config["file_groups"],
        config,
        custom_transform=custom_transform,
        sort_files=args.sort_files,
    )
    ptnb.process(args.no_iodide)


if __name__ == "__main__":
    main()
