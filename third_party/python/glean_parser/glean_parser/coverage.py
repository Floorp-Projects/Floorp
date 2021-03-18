# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Produce coverage reports from the raw information produced by the
`GLEAN_TEST_COVERAGE` feature.
"""

import json
from .metrics import ObjectTree
from pathlib import Path
import sys
from typing import Any, Dict, List, Optional, Sequence, Set


from . import parser
from . import util


def _outputter_codecovio(metrics: ObjectTree, output_path: Path):
    """
    Output coverage in codecov.io format as defined here:

        https://docs.codecov.io/docs/codecov-custom-coverage-format

    :param metrics: The tree of metrics, already annotated with coverage by
        `_annotate_coverage`.
    :param output_path: The file to output to.
    """
    coverage: Dict[str, List] = {}
    for category in metrics.values():
        for metric in category.values():
            defined_in = metric.defined_in
            if defined_in is not None:
                path = defined_in["filepath"]
                if path not in coverage:
                    with open(path) as fd:
                        nlines = len(list(fd.readlines()))
                    lines = [None] * nlines
                    coverage[path] = lines
                file_section = coverage[path]
                file_section[int(defined_in["line"])] = getattr(metric, "covered", 0)

    with open(output_path, "w") as fd:
        json.dump({"coverage": coverage}, fd)


OUTPUTTERS = {"codecovio": _outputter_codecovio}


def _annotate_coverage(metrics, coverage_entries):
    """
    Annotate each metric with whether it is covered. Sets the attribute
    `covered` to 1 on each metric that is covered.
    """
    mapping = {}
    for category in metrics.values():
        for metric in category.values():
            mapping[metric.identifier()] = metric

    for entry in coverage_entries:
        metric_id = _coverage_entry_to_metric_id(entry)
        if metric_id in mapping:
            mapping[metric_id].covered = 1


def _coverage_entry_to_metric_id(entry: str) -> str:
    """
    Convert a coverage entry to a metric id.

    Technically, the coverage entries are rkv database keys, so are not just
    the metric identifier. This extracts the metric identifier part out.
    """
    # If getting a glean error count, report it as covering the metric the
    # error occurred in, not the `glean.error.*` metric itself.
    if entry.startswith("glean.error."):
        entry = entry.split("/")[-1]
    # If a labeled metric, strip off the label part
    return entry.split("/")[0]


def _read_coverage_entries(coverage_reports: List[Path]) -> Set[str]:
    """
    Read coverage entries from one or more files, and deduplicates them.
    """
    entries = set()

    for coverage_report in coverage_reports:
        with open(coverage_report) as fd:
            for line in fd.readlines():
                entries.add(line.strip())

    return entries


def coverage(
    coverage_reports: List[Path],
    metrics_files: Sequence[Path],
    output_format: str,
    output_file: Path,
    parser_config: Optional[Dict[str, Any]] = None,
    file=sys.stderr,
) -> int:
    """
    Commandline helper for coverage.

    :param coverage_reports: List of coverage report files, output from the
        Glean SDK when the `GLEAN_TEST_COVERAGE` environment variable is set.
    :param metrics_files: List of Path objects to load metrics from.
    :param output_format: The coverage output format to produce. Must be one of
        `OUTPUTTERS.keys()`.
    :param output_file: Path to output coverage report to.
    :param parser_config: Parser configuration object, passed to
      `parser.parse_objects`.
    :return: Non-zero if there were any errors.
    """

    if parser_config is None:
        parser_config = {}

    if output_format not in OUTPUTTERS:
        raise ValueError(f"Unknown outputter {output_format}")

    metrics_files = util.ensure_list(metrics_files)

    all_objects = parser.parse_objects(metrics_files, parser_config)

    if util.report_validation_errors(all_objects):
        return 1

    entries = _read_coverage_entries(coverage_reports)

    _annotate_coverage(all_objects.value, entries)

    OUTPUTTERS[output_format](all_objects.value, output_file)

    return 0
