# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pathlib import Path
from mozperftest.metrics.notebook import PerftestNotebook


class MetricsStorage(object):
    """Holds data that is commonly used across all metrics layers.

    An instance of this class represents data for a given and output
    path and prefix.
    """

    def __init__(self, output_path, prefix, logger):
        self.prefix = prefix
        self.output_path = output_path
        self.stddata = None
        p = Path(output_path)
        p.mkdir(parents=True, exist_ok=True)
        self.results = []
        self.logger = logger

    def _parse_results(self, results):
        if isinstance(results, dict):
            return [results]
        res = []
        # XXX we need to embrace pathlib everywhere.
        if isinstance(results, (str, Path)):
            # Expecting a single path or a directory
            p = Path(results)
            if not p.exists():
                self.logger.warning("Given path does not exist: {}".format(results))
            elif p.is_dir():
                files = [f for f in p.glob("**/*.json") if not f.is_dir()]
                res.extend(self._parse_results(files))
            else:
                res.append(p.as_posix())
        if isinstance(results, list):
            # Expecting a list of paths
            for path in results:
                res.extend(self._parse_results(path))
        return res

    def set_results(self, results):
        """Processes and sets results provided by the metadata.

        `results` can be a path to a file or a directory. Every
        file is scanned and we build a list. Alternatively, it
        can be a mapping containing the results, in that case
        we just use it direcly, but keep it in a list.

        :param results list/dict/str: Path, or list of paths to the data (
            or the data itself in a dict) of the data to be processed.
        """
        self.results = self._parse_results(results)

    def get_standardized_data(
        self, group_name="firefox", transformer="SingleJsonRetriever", overwrite=False
    ):
        """Returns a parsed, standardized results data set.

        The dataset is computed once then cached unless overwrite is used.
        The transformer dictates how the data will be parsed, by default it uses
        a JSON transformer that flattens the dictionary while merging all the
        common metrics together.

        :param group_name str: The name for this results group.
        :param transformer str: The name of the transformer to use
            when parsing the data. Currently, only SingleJsonRetriever
            is available.
        :param overwrite str: if True, we recompute the results
        :return dict: Standardized notebook data with containing the
            requested metrics.
        """
        if not overwrite and self.stddata:
            return self.stddata

        # XXX Change config based on settings
        config = {
            "output": self.output_path,
            "prefix": self.prefix,
            "customtransformer": transformer,
            "file_groups": {group_name: self.results},
        }
        ptnb = PerftestNotebook(config["file_groups"], config, transformer)
        self.stddata = ptnb.process()
        return self.stddata

    def filtered_metrics(
        self,
        group_name="firefox",
        transformer="SingleJsonRetriever",
        overwrite=False,
        metrics=None,
    ):

        """Filters the metrics to only those that were requested by `metrics`.

        If metrics is Falsey (None, empty list, etc.) then no metrics
        will be filtered. The entries in metrics are pattern matched with
        the subtests in the standardized data (not a regular expression).
        For example, if "firstPaint" is in metrics, then all subtests which
        contain this string in their name, then they will be kept.

        :param metrics list: List of metrics to keep.
        :return dict: Standardized notebook data with containing the
            requested metrics.
        """
        results = self.get_standardized_data(
            group_name=group_name, transformer=transformer
        )["data"]

        if not metrics:
            return results

        newresults = []
        for res in results:
            if any([met in res["subtest"] for met in metrics]):
                newresults.append(res)

        return newresults


_metrics = {}


def filtered_metrics(
    metadata,
    path,
    prefix,
    group_name="firefox",
    transformer="SingleJsonRetriever",
    metrics=None,
):
    """Returns standardized data extracted from the metadata instance.

    We're caching an instance of MetricsStorage per metrics/storage
    combination and compute the data only once when this function is called.
    """
    key = path, prefix
    if key not in _metrics:
        storage = _metrics[key] = MetricsStorage(path, prefix, metadata)
        storage.set_results(metadata.get_result())

    return storage.filtered_metrics(
        group_name=group_name, transformer=transformer, metrics=metrics
    )
