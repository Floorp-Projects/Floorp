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
from glean_parser import util
from perfecthash import PerfectHash
from string_table import StringTable
from util import generate_metric_ids, generate_ping_ids, get_metrics

"""
We need to store several bits of information in the Perfect Hash Map Entry:

1. An index into the string table to check for string equality with a search key
   The perfect hash function will give false-positive for non-existent keys,
   so we need to verify these ourselves.
2. Type information to instantiate the correct C++ class
3. The metric's actual ID to lookup the underlying instance.
4. Whether the metric is a "submetric" (generated per-label for labeled_* metrics)
5. Whether the metric was registered at runtime

We have 64 bits to play with, so we dedicate:

1. 32 bit to the string table offset. More than enough for a large string table (~60M metrics).
2. 5 bit for the type. That allows for 32 metric types. We're not even close to that yet.
3. 25 bit for the metric ID. That allows for 33.5 million metrics. Let's not go there.
4. 1 bit for signifying that this metric is a submetric
5. 1 bit for signifying that this metric was registered at runtime

These values are interpolated into the template as well, so changing them here
ensures the generated C++ code follows.
If we ever need more bits for a part (e.g. when we add the 33rd metric type),
we figure out if either the string table indices or the range of possible IDs can be reduced
and adjust the constants below.
"""
ENTRY_WIDTH = 64
INDEX_BITS = 32
ID_BITS = 27  # Includes ID_SIGNAL_BITS
ID_SIGNAL_BITS = 2
TYPE_BITS = 5

PING_INDEX_BITS = 16

# Size of the PHF intermediate table.
# This ensures the algorithm finds empty slots in the buckets
# with the number of metrics we now have in-tree.
# toolkit/components/telemetry uses 1024, some others 512.
# See https://bugzilla.mozilla.org/show_bug.cgi?id=1822477
PHF_SIZE = 1024


def ping_entry(ping_id, ping_string_index):
    """
    The 2 pieces of information of a ping encoded into a single 32-bit integer.
    """
    assert ping_id < 2 ** (32 - PING_INDEX_BITS)
    assert ping_string_index < 2**PING_INDEX_BITS
    return ping_id << PING_INDEX_BITS | ping_string_index


def create_entry(metric_id, type_id, idx):
    """
    The 3 pieces of information of a metric encoded into a single 64-bit integer.
    """
    return metric_id << INDEX_BITS | type_id << (INDEX_BITS + ID_BITS) | idx


def metric_identifier(category, metric_name):
    """
    The metric's unique identifier, including the category and name
    """
    return f"{category}.{util.camelize(metric_name)}"


def type_name(obj):
    """
    Returns the C++ type to use for a given metric object.
    """

    if getattr(obj, "labeled", False):
        return "GleanLabeled"
    return "Glean" + util.Camelize(obj.type)


def subtype_name(obj):
    """
    Returns the subtype name for labeled metrics.
    (e.g. 'boolean' for 'labeled_boolean').
    Returns "" for non-labeled metrics.
    """
    if getattr(obj, "labeled", False):
        type = obj.type[8:]  # strips "labeled_" off the front
        return "Glean" + util.Camelize(type)
    return ""


def output_js(objs, output_fd_h, output_fd_cpp, options={}):
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

    if "pings" in objs:
        write_pings(
            {"pings": objs["pings"]},
            output_fd_cpp,
            "js_pings.jinja2",
            output_fd_h,
            "js_pings_h.jinja2",
        )
    else:
        write_metrics(
            get_metrics(objs), output_fd_cpp, "js.jinja2", output_fd_h, "js_h.jinja2"
        )


def write_metrics(objs, output_fd, template_filename, output_fd_h, template_filename_h):
    """
    Given a tree of objects `objs`, output metrics-only code for the JS API to the
    file-like object `output_fd` using template `template_filename`
    """

    template = util.get_jinja2_template(
        template_filename,
    )

    assert (
        INDEX_BITS + TYPE_BITS + ID_BITS <= ENTRY_WIDTH
    ), "INDEX_BITS, TYPE_BITS, or ID_BITS are larger than allowed"

    get_metric_id = generate_metric_ids(objs)
    # Mapping from a metric's identifier to the entry (metric ID | type id | index)
    metric_id_mapping = {}
    categories = []

    category_string_table = StringTable()
    metric_string_table = StringTable()
    # Mapping from a type name to its ID
    metric_type_ids = {}

    for category_name, objs in get_metrics(objs).items():
        category_camel = util.camelize(category_name)
        id = category_string_table.stringIndex(category_camel)
        categories.append((category_camel, id))

        for metric in objs.values():
            identifier = metric_identifier(category_camel, metric.name)
            metric_type_tuple = (type_name(metric), subtype_name(metric))
            if metric_type_tuple in metric_type_ids:
                type_id, _ = metric_type_ids[metric_type_tuple]
            else:
                type_id = len(metric_type_ids) + 1
                metric_type_ids[metric_type_tuple] = (type_id, metric.type)

            idx = metric_string_table.stringIndex(identifier)
            metric_id = get_metric_id(metric)
            entry = create_entry(metric_id, type_id, idx)
            metric_id_mapping[identifier] = entry

    # Create a lookup table for the metric categories only
    category_string_table = category_string_table.writeToString("gCategoryStringTable")
    category_map = [(bytearray(category, "ascii"), id) for (category, id) in categories]
    name_phf = PerfectHash(category_map, PHF_SIZE)
    category_by_name_lookup = name_phf.cxx_codegen(
        name="CategoryByNameLookup",
        entry_type="category_entry_t",
        lower_entry=lambda x: str(x[1]) + "ul",
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
        return_type="Maybe<uint32_t>",
        return_entry="return category_result_check(aKey, entry);",
    )

    # Create a lookup table for metric's identifiers.
    metric_string_table = metric_string_table.writeToString("gMetricStringTable")
    metric_map = [
        (bytearray(metric_name, "ascii"), metric_id)
        for (metric_name, metric_id) in metric_id_mapping.items()
    ]
    metric_phf = PerfectHash(metric_map, PHF_SIZE)
    metric_by_name_lookup = metric_phf.cxx_codegen(
        name="MetricByNameLookup",
        entry_type="metric_entry_t",
        lower_entry=lambda x: str(x[1]) + "ull",
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
        return_type="Maybe<uint32_t>",
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
            type_bits=TYPE_BITS,
            id_signal_bits=ID_SIGNAL_BITS,
            category_string_table=category_string_table,
            category_by_name_lookup=category_by_name_lookup,
            metric_string_table=metric_string_table,
            metric_by_name_lookup=metric_by_name_lookup,
        )
    )
    output_fd.write("\n")

    output_fd_h.write(
        util.get_jinja2_template(template_filename_h).render(
            index_bits=INDEX_BITS,
            id_bits=ID_BITS,
            type_bits=TYPE_BITS,
            id_signal_bits=ID_SIGNAL_BITS,
            num_categories=len(categories),
            num_metrics=len(metric_id_mapping.items()),
        )
    )
    output_fd_h.write("\n")


def write_pings(objs, output_fd, template_filename, output_fd_h, template_filename_h):
    """
    Given a tree of objects `objs`, output pings-only code for the JS API to the
    file-like object `output_fd` using template `template_filename`
    """

    template = util.get_jinja2_template(
        template_filename,
        filters=(),
    )

    ping_string_table = StringTable()
    get_ping_id = generate_ping_ids(objs)
    # The map of a ping's name to its entry (a combination of a monotonic
    # integer and its index in the string table)
    pings = {}
    for ping_name in objs["pings"].keys():
        ping_id = get_ping_id(ping_name)
        ping_camel = util.camelize(ping_name)
        pings[ping_camel] = ping_entry(
            ping_id, ping_string_table.stringIndex(ping_camel)
        )

    ping_map = [
        (bytearray(ping_name, "ascii"), ping_entry)
        for (ping_name, ping_entry) in pings.items()
    ]
    ping_string_table = ping_string_table.writeToString("gPingStringTable")
    ping_phf = PerfectHash(ping_map, PHF_SIZE)
    ping_by_name_lookup = ping_phf.cxx_codegen(
        name="PingByNameLookup",
        entry_type="ping_entry_t",
        lower_entry=lambda x: str(x[1]),
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
        return_type="Maybe<uint32_t>",
        return_entry="return ping_result_check(aKey, entry);",
    )

    output_fd.write(
        template.render(
            ping_index_bits=PING_INDEX_BITS,
            ping_by_name_lookup=ping_by_name_lookup,
            ping_string_table=ping_string_table,
        )
    )
    output_fd.write("\n")

    output_fd_h.write(
        util.get_jinja2_template(template_filename_h).render(
            num_pings=len(pings.items()),
        )
    )
    output_fd_h.write("\n")
