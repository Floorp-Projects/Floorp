# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pathlib import Path
from mozperftest.metrics.notebook import PerftestNotebook


class CommonMetricsSingleton(object):
    """CommonMetricsSingleton is a metrics class that contains code that is
    commonly used across all metrics classes.

    The metrics classes will be composed of this object, rather than inherit from it,
    for that reason this class is a singleton. Otherwise, the data would be recomputed
    for each consecutive metrics processor.
    """

    __initialized = False
    __instance = None

    def __new__(cls, *args, **kw):
        if not cls.__instance:
            cls.__instance = object.__new__(cls)
        return cls.__instance

    def __init__(self, results, warning, output="artifacts", prefix=""):
        """Initialize CommonMetricsSingleton object.

        :param results list/dict/str: Can be a single path to a result, a
            list of paths, or a dict containing the data itself.
        :param output str: Path of where the data will be stored.
        :param prefix str: Prefix the output files with this string.
        """
        if self.__initialized:
            return

        self.prefix = prefix
        self.output = output
        self.warning = warning
        self.stddata = None

        p = Path(output)
        p.mkdir(parents=True, exist_ok=True)

        self.results = self.parse_results(results)
        if not self.results:
            self.return_code = 1
            raise Exception("Could not find any results to process.")

        self.__class__.__initialized = True

    def parse_results(self, results):
        """This function determines the type of results, and processes
        it accordingly.

        If a single file path is given, the file path is resolved
        and returned. If a list is given, then all the files
        in that list (can include directories) are opened and returned.
        If a dictionary is returned, then nothing will be done to the
        results, but it will be returned within a list to keep the
        `self.results` variable type consistent.

        :param results list/dict/str: Path, or list of paths to the data (
            or the data itself in a dict) of the data to be processed.

        :return list: List of data objects to be processed.
        """
        res = []
        if isinstance(results, dict):
            res.append(results)
        elif isinstance(results, str) or isinstance(results, Path):
            # Expecting a single path or a directory
            p = Path(results)
            if not p.exists():
                self.warning("Given path does not exist: {}".format(results))
            elif p.is_dir():
                files = [f for f in p.glob("**/*.json") if not f.is_dir()]
                res.extend(self.parse_results(files))
            else:
                res.append(p.as_posix())
        elif isinstance(results, list):
            # Expecting a list of paths
            for path in results:
                res.extend(self.parse_results(path))
        return res

    def get_standardized_data(
        self, group_name="firefox", transformer="SingleJsonRetriever", overwrite=False
    ):
        """Returns a parsed, standardized results data set.

        If overwrite is True, then we will recompute the results,
        otherwise, the same dataset will be continuously returned after
        the first computation. The transformer dictates how the
        data will be parsed, by default it uses a JSON transformer
        that flattens the dictionary while merging all the common
        metrics together.

        :param group_name str: The name for this results group.
        :param transformer str: The name of the transformer to use
            when parsing the data. Currently, only SingleJsonRetriever
            is available.
        """
        if not overwrite and self.stddata:
            return self.stddata

        # XXX Change config based on settings
        config = {
            "output": self.output,
            "prefix": self.prefix,
            "customtransformer": transformer,
            "file_groups": {group_name: self.results},
        }
        ptnb = PerftestNotebook(config["file_groups"], config, transformer)
        self.stddata = ptnb.process()

        return self.stddata
