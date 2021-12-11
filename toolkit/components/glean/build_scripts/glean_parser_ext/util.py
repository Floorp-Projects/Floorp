# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Utitlity functions for the glean_parser-based code generator
"""


def generate_ping_ids(objs):
    """
    Return a lookup function for ping IDs per ping name.

    :param objs: A tree of objects as returned from `parser.parse_objects`.
    """

    if "pings" not in objs:

        def no_ping_ids_for_you():
            assert False

        return no_ping_ids_for_you

    # Ping ID 0 is reserved (but unused) right now.
    ping_id = 1

    ping_id_mapping = {}
    for ping_name in objs["pings"].keys():
        ping_id_mapping[ping_name] = ping_id
        ping_id += 1

    return lambda ping_name: ping_id_mapping[ping_name]


def generate_metric_ids(objs):
    """
    Return a lookup function for metric IDs per metric object.

    :param objs: A tree of metrics as returned from `parser.parse_objects`.
    """

    # Metric ID 0 is reserved (but unused) right now.
    metric_id = 1

    # Mapping from a tuple of (category name, metric name) to the metric's numeric ID
    metric_id_mapping = {}
    for category_name, metrics in objs.items():
        for metric in metrics.values():
            metric_id_mapping[(category_name, metric.name)] = metric_id
            metric_id += 1

    return lambda metric: metric_id_mapping[(metric.category, metric.name)]
