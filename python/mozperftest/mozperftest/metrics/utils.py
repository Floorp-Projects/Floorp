# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import json


def open_file(path):
    """Opens a file and returns its contents.

    :param path str: Path to the file, if it's a
        JSON, then a dict will be returned, otherwise,
        the raw contents (not split by line) will be
        returned.
    :return dict/str: Returns a dict for JSON data, and
        a str for any other type.
    """
    print("Reading %s" % path)
    with open(path) as f:
        if os.path.splitext(path)[-1] == ".json":
            return json.load(f)
        return f.read()


def write_json(data, path, file):
    """Writes data to a JSON file.

    :param data dict: Data to write.
    :param path str: Directory of where the data will be stored.
    :param file str: Name of the JSON file.
    :return str: Path to the output.
    """
    path = os.path.join(path, file)
    with open(path, "w+") as f:
        json.dump(data, f)
    return path


def filter_metrics(results, metrics):
    """Filters the metrics to only those that were requested by `metrics`.

    If metrics is Falsey (None, empty list, etc.) then no metrics
    will be filtered. The entries in metrics are pattern matched with
    the subtests in the standardized data (not a regular expression).
    For example, if "firstPaint" is in metrics, then all subtests which
    contain this string in their name, then they will be kept.

    :param results list: Standardized data from the notebook.
    :param metrics list: List of metrics to keep.
    :return dict: Standardized notebook data with containing the
        requested metrics.
    """
    if not metrics:
        return results

    newresults = []
    for res in results:
        if any([met in res["subtest"] for met in metrics]):
            newresults.append(res)

    return newresults
