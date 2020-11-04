# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Utitlity functions for the glean_parser-based code generator
"""


def generate_metric_ids(objs):
    """
    Return a lookup function for metric IDs per metric object.

    :param objs: A tree of metrics as returned from `parser.parse_objects`.
    """

    # Metric ID 0 is reserved (but unused) right now.
    metric_id = 1

    # Mapping from a tuple of (categoru name, metric name) to the metric's numeric ID
    metric_id_mapping = {}
    for category_name, metrics in objs.items():
        for metric in metrics.values():
            metric_id_mapping[(category_name, metric.name)] = metric_id
            metric_id += 1

    return lambda metric: metric_id_mapping[(metric.category, metric.name)]


IMPLEMENTED_CPP_TYPES = ["counter", "timespan", "uuid"]


def is_implemented_metric_type(typ):
    """
    Filter out some unimplemented metric types to avoid generating C++ code for them.
    Once all types are implemented this code will be removed.
    """
    return typ in IMPLEMENTED_CPP_TYPES
