# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re
import sys


from . import parser
from . import util

from yamllint.config import YamlLintConfig
from yamllint import linter


def _split_words(name):
    """
    Helper function to split words on either `.` or `_`.
    """
    return re.split("[._]", name)


def _hamming_distance(str1, str2):
    """
    Count the # of differences between strings str1 and str2,
    padding the shorter one with whitespace
    """

    diffs = 0
    if len(str1) < len(str2):
        str1, str2 = str2, str1
    len_dist = len(str1) - len(str2)
    str2 += " " * len_dist

    for ch1, ch2 in zip(str1, str2):
        if ch1 != ch2:
            diffs += 1
    return diffs


def check_common_prefix(category_name, metrics):
    """
    Check if all metrics begin with a common prefix.
    """
    metric_words = sorted([_split_words(metric.name) for metric in metrics])

    if len(metric_words) < 2:
        return

    first = metric_words[0]
    last = metric_words[-1]

    for i in range(min(len(first), len(last))):
        if first[i] != last[i]:
            break

    if i > 0:
        common_prefix = "_".join(first[:i])
        yield (
            "Within category '{}', all metrics begin with prefix "
            "'{}'. Remove prefixes and (possibly) rename category."
        ).format(category_name, common_prefix)


def check_unit_in_name(metric, parser_config={}):
    """
    The metric name ends in a unit.
    """
    TIME_UNIT_ABBREV = {
        "nanosecond": "ns",
        "microsecond": "us",
        "millisecond": "ms",
        "second": "s",
        "minute": "m",
        "hour": "h",
        "day": "d",
    }

    MEMORY_UNIT_ABBREV = {
        "byte": "b",
        "kilobyte": "kb",
        "megabyte": "mb",
        "gigabyte": "gb",
    }

    name_words = _split_words(metric.name)
    unit_in_name = name_words[-1]

    if hasattr(metric, "time_unit"):
        if (
            unit_in_name == TIME_UNIT_ABBREV.get(metric.time_unit.name)
            or unit_in_name == metric.time_unit.name
        ):
            yield (
                "Suffix '{}' is redundant with time_unit. " "Only include time_unit."
            ).format(unit_in_name)
        elif (
            unit_in_name in TIME_UNIT_ABBREV.keys()
            or unit_in_name in TIME_UNIT_ABBREV.values()
        ):
            yield (
                "Suffix '{}' doesn't match time_unit. "
                "Confirm the unit is correct and only include time_unit."
            ).format(unit_in_name)

    elif hasattr(metric, "memory_unit"):
        if (
            unit_in_name == MEMORY_UNIT_ABBREV.get(metric.memory_unit.name)
            or unit_in_name == metric.memory_unit.name
        ):
            yield (
                "Suffix '{}' is redundant with memory_unit. "
                "Only include memory_unit."
            ).format(unit_in_name)
        elif (
            unit_in_name in MEMORY_UNIT_ABBREV.keys()
            or unit_in_name in MEMORY_UNIT_ABBREV.values()
        ):
            yield (
                "Suffix '{}' doesn't match memory_unit. "
                "Confirm the unit is correct and only include memory_unit."
            ).format(unit_in_name)

    elif hasattr(metric, "unit"):
        if unit_in_name == metric.unit:
            yield (
                "Suffix '{}' is redundant with unit param. " "Only include unit."
            ).format(unit_in_name)


def check_category_generic(category_name, metrics):
    """
    The category name is too generic.
    """
    GENERIC_CATEGORIES = ["metrics", "events"]

    if category_name in GENERIC_CATEGORIES:
        yield "Category '{}' is too generic.".format(category_name)


def check_bug_number(metric, parser_config={}):
    number_bugs = [str(bug) for bug in metric.bugs if isinstance(bug, int)]

    if len(number_bugs):
        yield (
            "For bugs {}: "
            "Bug numbers are deprecated and should be changed to full URLs."
        ).format(", ".join(number_bugs))


def check_valid_in_baseline(metric, parser_config={}):
    allow_reserved = parser_config.get("allow_reserved", False)

    if not allow_reserved and "baseline" in metric.send_in_pings:
        yield (
            "The baseline ping is Glean-internal. "
            "User metrics should go into the 'metrics' ping or custom pings."
        )


def check_misspelled_pings(metric, parser_config={}):
    builtin_pings = ["metrics", "events"]

    for ping in metric.send_in_pings:
        for builtin in builtin_pings:
            distance = _hamming_distance(ping, builtin)
            if distance == 1:
                yield ("Ping '{}' seems misspelled. Did you mean '{}'?").format(
                    ping, builtin
                )


CATEGORY_CHECKS = {
    "COMMON_PREFIX": check_common_prefix,
    "CATEGORY_GENERIC": check_category_generic,
}


INDIVIDUAL_CHECKS = {
    "UNIT_IN_NAME": check_unit_in_name,
    "BUG_NUMBER": check_bug_number,
    "BASELINE_PING": check_valid_in_baseline,
    "MISSPELLED_PING": check_misspelled_pings,
}


def lint_metrics(objs, parser_config={}, file=sys.stderr):
    """
    Performs glinter checks on a set of metrics objects.

    :param objs: Tree of metric objects, as returns by `parser.parse_objects`.
    :param file: The stream to write errors to.
    :returns: List of nits.
    """
    nits = []
    for (category_name, metrics) in sorted(list(objs.items())):
        if category_name == "pings":
            continue

        for (check_name, check_func) in CATEGORY_CHECKS.items():
            if any(check_name in metric.no_lint for metric in metrics.values()):
                continue
            nits.extend(
                (check_name, category_name, msg)
                for msg in check_func(category_name, metrics.values())
            )

        for (metric_name, metric) in sorted(list(metrics.items())):
            for (check_name, check_func) in INDIVIDUAL_CHECKS.items():
                new_nits = list(check_func(metric, parser_config))
                if len(new_nits):
                    if check_name not in metric.no_lint:
                        nits.extend(
                            (check_name, ".".join([metric.category, metric.name]), msg)
                            for msg in new_nits
                        )
                else:
                    if (
                        check_name not in CATEGORY_CHECKS
                        and check_name in metric.no_lint
                    ):
                        nits.append(
                            (
                                "SUPERFLUOUS_NO_LINT",
                                ".".join([metric.category, metric.name]),
                                (
                                    "Superfluous no_lint entry '{}'. "
                                    "Please remove it."
                                ).format(check_name),
                            )
                        )

    if len(nits):
        print("Sorry, Glean found some glinter nits:", file=file)
        for check_name, name, msg in nits:
            print("{}: {}: {}".format(check_name, name, msg), file=file)
        print("", file=file)
        print("Please fix the above nits to continue.", file=file)
        print(
            "To disable a check, add a `no_lint` parameter "
            "with a list of check names to disable.\n"
            "This parameter can appear with each individual metric, or at the "
            "top-level to affect the entire file.",
            file=file,
        )

    return nits


def lint_yaml_files(input_filepaths, file=sys.stderr):
    """
    Performs glinter YAML lint on a set of files.

    :param input_filepaths: List of input files to lint.
    :param file: The stream to write errors to.
    :returns: List of nits.
    """

    nits = []
    for path in input_filepaths:
        # yamllint needs both the file content and the path.
        file_content = None
        with path.open("r") as fd:
            file_content = fd.read()

        problems = linter.run(file_content, YamlLintConfig("extends: default"), path)
        nits.extend(p for p in problems)

    if len(nits):
        print("Sorry, Glean found some glinter nits:", file=file)
        for p in nits:
            print("{} ({}:{}) - {}".format(path, p.line, p.column, p.message))
        print("", file=file)
        print("Please fix the above nits to continue.", file=file)

    return nits


def glinter(input_filepaths, parser_config={}, file=sys.stderr):
    """
    Commandline helper for glinter.

    :param input_filepaths: List of Path objects to load metrics from.
    :param parser_config: Parser configuration objects, passed to
      `parser.parse_objects`.
    :param file: The stream to write the errors to.
    :return: Non-zero if there were any glinter errors.
    """
    if lint_yaml_files(input_filepaths, file=file):
        return 1

    objs = parser.parse_objects(input_filepaths, parser_config)

    if util.report_validation_errors(objs):
        return 1

    if lint_metrics(objs.value, parser_config=parser_config, file=file):
        return 1

    print("✨ Your metrics are Glean! ✨", file=file)
    return 0
