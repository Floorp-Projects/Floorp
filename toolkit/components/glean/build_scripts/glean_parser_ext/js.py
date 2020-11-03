# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate C++ code for the JavaScript API for metrics.

The code for the JavaScript API is a bit special in that we only generate C++ code,
string tables and mapping functions.
The rest is handled by the WebIDL and XPIDL implementation
that uses this code to look up metrics by name.
"""

import jinja2
from perfecthash import PerfectHash
from string_table import StringTable

from util import generate_metric_ids, is_implemented_metric_type
from glean_parser import util

"""
We need to store several bits of information in the Perfect Hash Map Entry:

1. An index into the string table to check for string equality with a search key
   The perfect hash function will give false-positive for non-existent keys,
   so we need to verify these ourselves.
2. Type information to instantiate the correct C++ class
3. The metric's actual ID to lookup the underlying instance.

We have 64 bits to play with, so we dedicate:

1. 32 bit to the string table offset. More than enough for a large string table (~60M metrics).
2. 5 bit for the type. That allows for 32 metric types. We're not even close to that yet.
3. 27 bit for the metric ID. That allows for 130 million metrics. Let's not go there.

These values are interpolated into the template as well, so changing them here
ensures the generated C++ code follows.
If we ever need more bits for a part (e.g. when we add the 33rd metric type),
we figure out if either the string table indices or the range of possible IDs can be reduced
and adjust the constants below.
"""
ENTRY_WIDTH = 64
INDEX_BITS = 32
ID_BITS = 27


def create_entry(metric_id, type_id, idx):
    """
    The 3 pieces of information of a metric encoded into a single 64-bit integer.
    """
    return metric_id << INDEX_BITS | type_id << (INDEX_BITS + ID_BITS) | idx


def metric_identifier(category, metric_name):
    """
    The metric's unique identifier, including the category and name
    """
    return f"{category}.{metric_name}"


def type_name(type):
    """
    Returns the C++ type to use for a given metric object.
    """

    return "Glean" + util.Camelize(type)


def output_js(objs, output_fd, options={}):
    """
    Given a tree of objects, output code for the JS API to the file-like object `output_fd`.

    :param objs: A tree of objects (metrics and pings) as returned from
    `parser.parse_objects`.
    :param output_fd: Writeable file to write the output to.
    :param options: options dictionary.
    """

    # Monkeypatch util.get_jinja2_template to find templates nearby

    def get_local_template(template_name, filters=()):
        env = jinja2.Environment(
            loader=jinja2.PackageLoader("js", "templates"),
            trim_blocks=True,
            lstrip_blocks=True,
        )
        env.filters["Camelize"] = util.Camelize
        for filter_name, filter_func in filters:
            env.filters[filter_name] = filter_func
        return env.get_template(template_name)

    util.get_jinja2_template = get_local_template
    template_filename = "js.jinja2"

    template = util.get_jinja2_template(
        template_filename,
        filters=(
            ("type_name", type_name),
            ("is_implemented_type", is_implemented_metric_type),
        ),
    )

    assert (
        INDEX_BITS + ID_BITS < ENTRY_WIDTH
    ), "INDEX_BITS or ID_BITS are larger than allowed"

    get_metric_id = generate_metric_ids(objs)
    # Mapping from a metric's identifier to the entry (metric ID | type id | index)
    metric_id_mapping = {}
    categories = []

    category_string_table = StringTable()
    metric_string_table = StringTable()
    # Mapping from a type name to its ID
    metric_type_ids = {}

    for category_name, objs in objs.items():
        id = category_string_table.stringIndex(category_name)
        categories.append((category_name, id))

        for metric in objs.values():
            identifier = metric_identifier(category_name, metric.name)
            if metric.type in metric_type_ids:
                type_id = metric_type_ids[metric.type]
            else:
                type_id = len(metric_type_ids) + 1
                metric_type_ids[metric.type] = type_id

            idx = metric_string_table.stringIndex(identifier)
            metric_id = get_metric_id(metric)
            entry = create_entry(metric_id, type_id, idx)
            metric_id_mapping[identifier] = entry

    # Create a lookup table for the metric categories only
    category_string_table = category_string_table.writeToString("gCategoryStringTable")
    category_map = [(bytearray(category, "ascii"), id) for (category, id) in categories]
    name_phf = PerfectHash(category_map, 64)
    category_by_name_lookup = name_phf.cxx_codegen(
        name="CategoryByNameLookup",
        entry_type="category_entry_t",
        lower_entry=lambda x: str(x[1]),
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
        return_type="static Maybe<uint32_t>",
        return_entry="return category_result_check(aKey, entry);",
    )

    # Create a lookup table for metric's identifiers.
    metric_string_table = metric_string_table.writeToString("gMetricStringTable")
    metric_map = [
        (bytearray(metric_name, "ascii"), metric_id)
        for (metric_name, metric_id) in metric_id_mapping.items()
    ]
    metric_phf = PerfectHash(metric_map, 64)
    metric_by_name_lookup = metric_phf.cxx_codegen(
        name="MetricByNameLookup",
        entry_type="metric_entry_t",
        lower_entry=lambda x: str(x[1]),
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
        return_type="static Maybe<uint32_t>",
        return_entry="return metric_result_check(aKey, entry);",
    )

    output_fd.write(
        template.render(
            categories=categories,
            metric_id_mapping=metric_id_mapping,
            metric_type_ids=metric_type_ids,
            entry_width=ENTRY_WIDTH,
            index_bits=INDEX_BITS,
            id_bits=ID_BITS,
            category_string_table=category_string_table,
            category_by_name_lookup=category_by_name_lookup,
            metric_string_table=metric_string_table,
            metric_by_name_lookup=metric_by_name_lookup,
        )
    )
    output_fd.write("\n")
