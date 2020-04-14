# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pathlib import Path
from mozperftest.metrics.utils import open_file


class CommonMetrics(object):
    """CommonMetrics is a metrics class that contains code that is
    commonly used across all metrics classes.

    The metrics classes will be composed of this objcet, rather than inherit from it.
    """

    def __init__(self, results, output="artifacts", prefix=""):
        """Initialize CommonMetrics object.

        :param results list/dict/str: Can be a single path to a result, a
            list of paths, or a dict containing the data itself.
        :param output str: Path of where the data will be stored.
        :param prefix str: Prefix the output files with this string.
        """
        self.prefix = prefix
        self.output = output

        p = Path(output)
        p.mkdir(parents=True, exist_ok=True)

        self.results = self.parse_results(results)
        if not self.results:
            self.return_code = 1
            raise Exception("Could not find any results to process.")

    def parse_results(self, results):
        """This function determines the type of results, and processes
        it accordingly.

        If a single file path is given, the file is opened
        and the data is returned. If a list is given, then all the files
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
                files = [f for f in p.glob("**/*") if not f.is_dir()]
                res.extend(self.parse_results(files))
            else:
                # XXX here we get browsertime.json as well as mp4s when
                # recording videos
                # XXX for now we skip binary files
                if str(p).endswith("browsertime.json"):
                    res.append(open_file(p.as_posix()))
        elif isinstance(results, list):
            # Expecting a list of paths
            for path in results:
                res.extend(self.parse_results(path))
        return res
