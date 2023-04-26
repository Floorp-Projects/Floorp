# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate Rust code for metrics.
"""

import enum
import json

import jinja2
from glean_parser import util
from glean_parser.metrics import CowString, Event, Rate
from util import generate_metric_ids, generate_ping_ids, get_metrics

from js import ID_BITS, ID_SIGNAL_BITS

# The list of all args to CommonMetricData.
# No particular order is required, but I have these in common_metric_data.rs
# order just to be organized.
common_metric_data_args = [
    "name",
    "category",
    "send_in_pings",
    "lifetime",
    "disabled",
    "dynamic_label",
]


def rust_datatypes_filter(value):
    """
    A Jinja2 filter that renders Rust literals.

    Based on Python's JSONEncoder, but overrides:
      - dicts and sets to raise an error
      - sets to vec![] (used in labels)
      - enums to become Class::Value
      - lists to vec![] (used in send_in_pings)
      - null to None
      - strings to "value".into()
      - Rate objects to a CommonMetricData initializer
        (for external Denominators' Numerators lists)
    """

    class RustEncoder(json.JSONEncoder):
        def iterencode(self, value):
            if isinstance(value, dict):
                raise ValueError("RustEncoder doesn't know dicts {}".format(str(value)))
            elif isinstance(value, enum.Enum):
                yield (value.__class__.__name__ + "::" + util.Camelize(value.name))
            elif isinstance(value, set):
                yield from self.iterencode(sorted(list(value)))
            elif isinstance(value, list):
                yield "vec!["
                first = True
                for subvalue in list(value):
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "]"
            elif value is None:
                yield "None"
            # CowString is also a 'str' but is a special case.
            # Ensure its case is handled before str's (below).
            elif isinstance(value, CowString):
                yield f'::std::borrow::Cow::from("{value.inner}")'
            elif isinstance(value, str):
                yield '"' + value + '".into()'
            elif isinstance(value, Rate):
                yield "CommonMetricData {"
                for arg_name in common_metric_data_args:
                    if hasattr(value, arg_name):
                        yield f"{arg_name}: "
                        yield from self.iterencode(getattr(value, arg_name))
                        yield ", "
                yield " ..Default::default()}"
            else:
                yield from super().iterencode(value)

    return "".join(RustEncoder().iterencode(value))


def ctor(obj):
    """
    Returns the scope and name of the constructor to use for a metric object.
    Necessary because LabeledMetric<T> is constructed using LabeledMetric::new
    not LabeledMetric<T>::new
    """
    if getattr(obj, "labeled", False):
        return "LabeledMetric::new"
    return class_name(obj.type) + "::new"


def type_name(obj):
    """
    Returns the Rust type to use for a given metric or ping object.
    """

    if getattr(obj, "labeled", False):
        label_enum = "super::DynamicLabel"
        if obj.labels and len(obj.labels):
            label_enum = f"{util.Camelize(obj.name)}Label"
        return f"LabeledMetric<Labeled{class_name(obj.type)}, {label_enum}>"
    generate_enums = getattr(obj, "_generate_enums", [])  # Extra Keys? Reasons?
    if len(generate_enums):
        for name, _ in generate_enums:
            if not len(getattr(obj, name)) and isinstance(obj, Event):
                return class_name(obj.type) + "<NoExtraKeys>"
            else:
                # we always use the `extra` suffix,
                # because we only expose the new event API
                suffix = "Extra"
                return "{}<{}>".format(
                    class_name(obj.type), util.Camelize(obj.name) + suffix
                )
    return class_name(obj.type)


def extra_type_name(typ: str) -> str:
    """
    Returns the corresponding Rust type for event's extra key types.
    """

    if typ == "boolean":
        return "bool"
    elif typ == "string":
        return "String"
    elif typ == "quantity":
        return "u32"
    else:
        return "UNSUPPORTED"


def class_name(obj_type):
    """
    Returns the Rust class name for a given metric or ping type.
    """
    if obj_type == "ping":
        return "Ping"
    if obj_type.startswith("labeled_"):
        obj_type = obj_type[8:]
    return util.Camelize(obj_type) + "Metric"


def extra_keys(allowed_extra_keys):
    """
    Returns the &'static [&'static str] ALLOWED_EXTRA_KEYS for impl ExtraKeys
    """
    return "&[" + ", ".join(map(lambda key: '"' + key + '"', allowed_extra_keys)) + "]"


def output_rust(objs, output_fd, ping_names_by_app_id, options={}):
    """
    Given a tree of objects, output Rust code to the file-like object `output_fd`.

    :param objs: A tree of objects (metrics and pings) as returned from
    `parser.parse_objects`.
    :param output_fd: Writeable file to write the output to.
    :param ping_names_by_app_id: A map of app_ids to lists of ping names.
                                 Used to determine which custom pings to register.
    :param options: options dictionary, presently unused.
    """

    # Monkeypatch util.snake_case for the templates to use
    util.snake_case = lambda value: value.replace(".", "_").replace("-", "_")
    # Monkeypatch util.get_jinja2_template to find templates nearby

    def get_local_template(template_name, filters=()):
        env = jinja2.Environment(
            loader=jinja2.PackageLoader("rust", "templates"),
            trim_blocks=True,
            lstrip_blocks=True,
        )
        env.filters["camelize"] = util.camelize
        env.filters["Camelize"] = util.Camelize
        for filter_name, filter_func in filters:
            env.filters[filter_name] = filter_func
        return env.get_template(template_name)

    util.get_jinja2_template = get_local_template
    get_metric_id = generate_metric_ids(objs)
    get_ping_id = generate_ping_ids(objs)

    # Map from a tuple (const, typ) to an array of tuples (id, path)
    # where:
    #   const: The Rust constant name to be used for the lookup map
    #   typ:   The metric type to be stored in the lookup map
    #   id:    The numeric metric ID
    #   path:  The fully qualified path to the metric object in Rust
    #
    # This map is only filled for metrics, not for pings.
    #
    # Example:
    #
    #   ("COUNTERS", "CounterMetric") -> [(1, "test_only::clicks"), ...]
    objs_by_type = {}

    # Map from a metric ID to the fully qualified path of the event object in Rust.
    # Required for the special handling of event lookups.
    #
    # Example:
    #
    #   17 -> "test_only::an_event"
    events_by_id = {}

    # Map from a labeled type (e.g. "counter") to a map from metric ID to the
    # fully qualified path of the labeled metric object in Rust paired with
    # whether the labeled metric has an enum.
    # Required for the special handling of labeled metric lookups.
    #
    # Example:
    #
    #   "counter" -> 42 -> ("test_only::mabels_kitchen_counters", false)
    labeleds_by_id_by_type = {}

    if "pings" in objs:
        template_filename = "rust_pings.jinja2"
        objs = {"pings": objs["pings"]}
    else:
        template_filename = "rust.jinja2"
        objs = get_metrics(objs)
        for category_name, category_value in objs.items():
            for metric in category_value.values():
                # The constant is all uppercase and suffixed by `_MAP`
                const_name = util.snake_case(metric.type).upper() + "_MAP"
                typ = type_name(metric)
                key = (const_name, typ)

                metric_name = util.snake_case(metric.name)
                category_snake = util.snake_case(category_name)
                full_path = f"{category_snake}::{metric_name}"

                if metric.type == "event":
                    events_by_id[get_metric_id(metric)] = full_path
                    continue

                if getattr(metric, "labeled", False):
                    labeled_type = metric.type[8:]
                    if labeled_type not in labeleds_by_id_by_type:
                        labeleds_by_id_by_type[labeled_type] = {}
                    labeleds_by_id_by_type[labeled_type][get_metric_id(metric)] = (
                        full_path,
                        metric.labels and len(metric.labels),
                    )
                    continue

                if key not in objs_by_type:
                    objs_by_type[key] = []
                objs_by_type[key].append((get_metric_id(metric), full_path))

    # Now for the modules for each category.
    template = util.get_jinja2_template(
        template_filename,
        filters=(
            ("rust", rust_datatypes_filter),
            ("snake_case", util.snake_case),
            ("type_name", type_name),
            ("extra_type_name", extra_type_name),
            ("ctor", ctor),
            ("extra_keys", extra_keys),
            ("metric_id", get_metric_id),
            ("ping_id", get_ping_id),
        ),
    )

    output_fd.write(
        template.render(
            all_objs=objs,
            common_metric_data_args=common_metric_data_args,
            metric_by_type=objs_by_type,
            extra_args=util.extra_args,
            events_by_id=events_by_id,
            labeleds_by_id_by_type=labeleds_by_id_by_type,
            submetric_bit=ID_BITS - ID_SIGNAL_BITS,
            ping_names_by_app_id=ping_names_by_app_id,
        )
    )
    output_fd.write("\n")
