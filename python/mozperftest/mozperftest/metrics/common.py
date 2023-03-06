# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import defaultdict
from pathlib import Path

from mozperftest.metrics.exceptions import (
    MetricsMissingResultsError,
    MetricsMultipleTransformsError,
)
from mozperftest.metrics.notebook import PerftestETL
from mozperftest.metrics.utils import metric_fields, validate_intermediate_results

COMMON_ARGS = {
    "metrics": {
        "type": metric_fields,
        "nargs": "*",
        "default": [],
        "help": "The metrics that should be retrieved from the data.",
    },
    "prefix": {"type": str, "default": "", "help": "Prefix used by the output files."},
    "split-by": {
        "type": str,
        "default": None,
        "help": "A metric name to use for splitting the data. For instance, "
        "using browserScripts.pageinfo.url will split the data by the unique "
        "URLs that are found.",
    },
    "simplify-names": {
        "action": "store_true",
        "default": False,
        "help": "If set, metric names will be simplified to a single word. The PerftestETL "
        "combines dictionary keys by `.`, and the final key contains that value of the data. "
        "That final key becomes the new name of the metric.",
    },
    "simplify-exclude": {
        "nargs": "*",
        "default": ["statistics"],
        "help": "When renaming/simplifying metric names, entries with these strings "
        "will be ignored and won't get simplified. These options are only used when "
        "--simplify-names is set.",
    },
    "transformer": {
        "type": str,
        "default": None,
        "help": "The path to the file containing the custom transformer, "
        "or the module to import along with the class name, "
        "e.g. mozperftest.test.xpcshell:XpcShellTransformer",
    },
}


class MetricsStorage(object):
    """Holds data that is commonly used across all metrics layers.

    An instance of this class represents data for a given and output
    path and prefix.
    """

    def __init__(self, output_path, prefix, logger):
        self.prefix = prefix
        self.output_path = output_path
        self.stddata = {}
        self.ptnb_config = {}
        self.results = []
        self.logger = logger

        p = Path(output_path)
        p.mkdir(parents=True, exist_ok=True)

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

        :param results list/dict/str: Path, or list of paths to the data
            (or the data itself in a dict) of the data to be processed.
        """
        # Parse the results into files (for now) and the settings
        self.results = defaultdict(lambda: defaultdict(list))
        self.settings = defaultdict(dict)
        for res in results:
            # Ensure that the results are valid before continuing
            validate_intermediate_results(res)

            name = res["name"]
            if isinstance(res["results"], dict):
                # XXX Implement subtest based parsing
                raise NotImplementedError(
                    "Subtest-based processing is not implemented yet"
                )

            # Merge all entries with the same name into one
            # result, if separation is needed use unique names
            self.results[name]["files"].extend(self._parse_results(res["results"]))

            suite_settings = self.settings[name]
            for key, val in res.items():
                if key == "results":
                    continue
                suite_settings[key] = val

            # Check the transform definitions
            currtrfm = self.results[name]["transformer"]
            if not currtrfm:
                self.results[name]["transformer"] = res.get(
                    "transformer", "SingleJsonRetriever"
                )
            elif currtrfm != res.get("transformer", "SingleJsonRetriever"):
                raise MetricsMultipleTransformsError(
                    f"Only one transformer allowed per data name! Found multiple for {name}: "
                    f"{[currtrfm, res['transformer']]}"
                )

            # Get the transform options if available
            self.results[name]["options"] = res.get("transformer-options", {})

        if not self.results:
            self.return_code = 1
            raise MetricsMissingResultsError("Could not find any results to process.")

    def get_standardized_data(self, group_name="firefox", transformer=None):
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
        if self.stddata:
            return self.stddata

        for data_type, data_info in self.results.items():
            tfm = transformer if transformer is not None else data_info["transformer"]
            prefix = data_type
            if self.prefix:
                prefix = "{}-{}".format(self.prefix, data_type)

            # Primarily used to store the transformer used on the data
            # so that it can also be used for generating things
            # like summary values for suites, and subtests.
            self.ptnb_config[data_type] = {
                "output": self.output_path,
                "prefix": prefix,
                "custom_transformer": tfm,
                "file_groups": {data_type: data_info["files"]},
            }

            ptnb = PerftestETL(
                file_groups=self.ptnb_config[data_type]["file_groups"],
                config=self.ptnb_config[data_type],
                prefix=self.prefix,
                logger=self.logger,
                custom_transform=tfm,
            )
            r = ptnb.process(**data_info["options"])
            self.stddata[data_type] = r["data"]

        return self.stddata

    def filtered_metrics(
        self,
        group_name="firefox",
        transformer=None,
        metrics=None,
        exclude=None,
        split_by=None,
        simplify_names=False,
        simplify_exclude=["statistics"],
    ):
        """Filters the metrics to only those that were requested by `metrics`.

        If metrics is Falsey (None, empty list, etc.) then no metrics
        will be filtered. The entries in metrics are pattern matched with
        the subtests in the standardized data (not a regular expression).
        For example, if "firstPaint" is in metrics, then all subtests which
        contain this string in their name will be kept.

        :param metrics list: List of metrics to keep.
        :param exclude list: List of string matchers to exclude from the metrics
            gathered/reported.
        :param split_by str: The name of a metric to use to split up data by.
        :param simplify_exclude list: List of string matchers to exclude
            from the naming simplification process.
        :return dict: Standardized notebook data containing the
            requested metrics.
        """
        results = self.get_standardized_data(
            group_name=group_name, transformer=transformer
        )
        if not metrics:
            return results
        if not exclude:
            exclude = []
        if not simplify_exclude:
            simplify_exclude = []

        # Get the field to split the results by (if any)
        if split_by is not None:
            splitting_entry = None
            for data_type, data_info in results.items():
                for res in data_info:
                    if split_by in res["subtest"]:
                        splitting_entry = res
                        break
            if splitting_entry is not None:
                split_by = defaultdict(list)
                for c, entry in enumerate(splitting_entry["data"]):
                    split_by[entry["value"]].append(c)

        # Filter metrics
        filtered = {}
        for data_type, data_info in results.items():
            newresults = []
            for res in data_info:
                if any([met["name"] in res["subtest"] for met in metrics]) and not any(
                    [met in res["subtest"] for met in exclude]
                ):
                    res["transformer"] = self.ptnb_config[data_type][
                        "custom_transformer"
                    ]
                    newresults.append(res)
            filtered[data_type] = newresults

        # Simplify the filtered metric names
        if simplify_names:

            def _simplify(name):
                if any([met in name for met in simplify_exclude]):
                    return None
                return name.split(".")[-1]

            self._alter_name(filtered, res, filter=_simplify)

        # Split the filtered results
        if split_by is not None:
            newfilt = {}
            total_iterations = sum([len(inds) for _, inds in split_by.items()])
            for data_type in filtered:
                if not filtered[data_type]:
                    # Ignore empty data types
                    continue

                newresults = []
                newfilt[data_type] = newresults
                for split, indices in split_by.items():
                    for res in filtered[data_type]:
                        if len(res["data"]) != total_iterations:
                            # Skip data that cannot be split
                            continue
                        splitres = {key: val for key, val in res.items()}
                        splitres["subtest"] += " " + split
                        splitres["data"] = [res["data"][i] for i in indices]
                        splitres["transformer"] = self.ptnb_config[data_type][
                            "custom_transformer"
                        ]

                        newresults.append(splitres)

            filtered = newfilt

        return filtered

    def _alter_name(self, filtered, res, filter):
        previous = []
        for data_type, data_info in filtered.items():
            for res in data_info:
                new = filter(res["subtest"])
                if new is None:
                    continue
                if new in previous:
                    self.logger.warning(
                        f"Another metric which ends with `{new}` was already found. "
                        f"{res['subtest']} will not be simplified."
                    )
                    continue
                res["subtest"] = new
                previous.append(new)


_metrics = {}


def filtered_metrics(
    metadata,
    path,
    prefix,
    group_name="firefox",
    transformer=None,
    metrics=None,
    settings=False,
    exclude=None,
    split_by=None,
    simplify_names=False,
    simplify_exclude=["statistics"],
):
    """Returns standardized data extracted from the metadata instance.

    We're caching an instance of MetricsStorage per metrics/storage
    combination and compute the data only once when this function is called.
    """
    key = path, prefix
    if key not in _metrics:
        storage = _metrics[key] = MetricsStorage(path, prefix, metadata)
        storage.set_results(metadata.get_results())
    else:
        storage = _metrics[key]

    results = storage.filtered_metrics(
        group_name=group_name,
        transformer=transformer,
        metrics=metrics,
        exclude=exclude,
        split_by=split_by,
        simplify_names=simplify_names,
        simplify_exclude=simplify_exclude,
    )

    # XXX returning two different types is a problem
    # in case settings is false, we should return None for it
    # and always return a 2-tuple
    if settings:
        return results, storage.settings
    return results
