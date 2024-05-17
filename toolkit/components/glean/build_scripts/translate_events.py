# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Create a Legacy Telemetry event definition for the provided, named Glean event metric.
"""

import re
import sys
from os import path
from pathlib import Path
from typing import Sequence

import yaml
from glean_parser import parser, util

TC_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TC_ROOT_PATH)
# The parsers live in a subdirectory of "build_scripts", account for that.
# NOTE: if the parsers are moved, this logic will need to be updated.
sys.path.append(path.join(TC_ROOT_PATH, "telemetry", "build_scripts"))

from mozparsers.parse_events import convert_to_cpp_identifier  # noqa: E402

bug_number_pattern = re.compile(r"\d+")


class IndentingDumper(yaml.Dumper):
    def increase_indent(self, flow=False, indentless=False):
        return super(IndentingDumper, self).increase_indent(flow, False)


def get_bug_number_from_url(url: str) -> int:
    bug = bug_number_pattern.search(url)
    # Python lacks a safe cast, so we will return 1 if we fail to bubble up.
    if bug is not None:
        try:
            bug = int(bug[0])
        except TypeError:
            print(f"Failed to parse {bug[0]} to an integer")
            return 1
        return bug
    print(f"Failed to find a valid bug in the url {url}")
    return 1


def create_legacy_mirror_def(category_name: str, metric_name: str, event_objects: list):
    event_cpp_enum = convert_to_cpp_identifier(category_name, ".") + "_"
    event_cpp_enum += convert_to_cpp_identifier(metric_name, "_") + "_"
    event_cpp_enum += convert_to_cpp_identifier(event_objects[0], "_")

    print(
        f"""The Glean event {category_name}.{metric_name} has generated the {event_cpp_enum} Legacy Telemetry event. To link Glean to Legacy, please include in {category_name}.{metric_name}'s definition the following property:
telemetry_mirror: {event_cpp_enum}
See https://firefox-source-docs.mozilla.org/toolkit/components/glean/user/gifft.html#the-telemetry-mirror-property-in-metrics-yaml
for more information.
"""
    )


def translate_event(
    event_category_and_name: str,
    append: bool,
    metrics_files: Sequence[Path],
    legacy_yaml_file: Path,
) -> int:
    """
    Commandline helper for translating Glean events to Legacy.

    :param event_name: the event to look for
    :param metrics_files: List of Path objects to load metrics from.
    :return: Non-zero if there were any errors.
    """

    # Event objects are a Legacy Telemetry event field that is rarely used. We
    # always broadcast into a single pre-defined object.
    event_objects = ["events"]

    if event_category_and_name is not None:
        *event_category, event_name = event_category_and_name.rsplit(".", maxsplit=1)
    else:
        print("Please provide an event (in category.metricName format) to translate.")
        return 1

    if len(event_category) > 0:
        event_category = util.snake_case(event_category[0])
        event_name = util.snake_case(event_name)
    else:
        print(
            f"Your event '{event_category_and_name}' did not conform to the a.category.a_metric_name format."
        )
        return 1

    metrics_files = util.ensure_list(metrics_files)

    # Accept any value of expires.
    parser_options = {
        "allow_reserved": True,
        "custom_is_expired": lambda expires: False,
        "custom_validate_expires": lambda expires: True,
    }
    all_objects = parser.parse_objects(metrics_files, parser_options)

    if util.report_validation_errors(all_objects):
        return 1

    for category_name, metrics in all_objects.value.items():
        for metric in metrics.values():
            metric_name = util.snake_case(metric.name)
            category_snake = util.snake_case(category_name)

            if metric_name != event_name or category_snake != event_category:
                continue

            if metric.type != "event":
                print(
                    f"Metric {event_category_and_name} was found, but was a {metric.type} metric, not an event metric."
                )
                return 1

            bugs_list = [get_bug_number_from_url(m) for m in metric.bugs]
            # Bail out if there was a parse error (error printed in get_bug_number_from_url)
            if 1 in bugs_list:
                return 1

            metric_dict = {
                "objects": event_objects,
                "description": str(metric.description).strip(),
                "bug_numbers": bugs_list,
                "notification_emails": metric.notification_emails,
                "expiry_version": str(metric.expires),
                "products": ["firefox"],
                "record_in_processes": ["all"],
            }

            if len(metric.extra_keys.items()) > 0:
                # Convert extra keys into a nested dictionary that YAML can understand
                extra_keys = {}
                for extra_key_pair in metric.extra_keys.items():
                    description = (
                        extra_key_pair[1]["description"].replace("\n", " ").strip()
                    )
                    extra_keys[extra_key_pair[0]] = description

                metric_dict["extra_keys"] = extra_keys

            metric_dict = {metric_name: metric_dict}
            metric_dict = {category_snake: metric_dict}

            metric_yaml = yaml.dump(
                metric_dict,
                default_flow_style=False,
                width=78,
                indent=2,
                Dumper=IndentingDumper,
            )

            if append:
                with open(legacy_yaml_file, "a") as file:
                    print("", file=file)
                    print(metric_yaml, file=file)
                print(
                    f"Apended {event_category_and_name} to the Legacy Events.yaml file. Please confirm the details by opening"
                )
                print(legacy_yaml_file)
                print("and checking that all fields are correct.")

                create_legacy_mirror_def(category_snake, event_name, event_objects)
                return 0

            print(metric_yaml)
            create_legacy_mirror_def(category_snake, event_name, event_objects)

    return 0
