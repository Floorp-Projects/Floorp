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
    """

    class RustEncoder(json.JSONEncoder):
        def iterencode(self, value):
            if isinstance(value, dict):
                raise ValueError("RustEncoder doesn't know dicts {}".format(str(value)))
            elif isinstance(value, enum.Enum):
                yield (value.__class__.__name__ + "::" + util.Camelize(value.name))
            elif isinstance(value, set):
                yield "vec!["
                first = True
                for subvalue in sorted(list(value)):
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "]"
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
            elif isinstance(value, str):
                yield '"' + value + '".into()'
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
        return "LabeledMetric<{}>".format(class_name(obj.type))
    generate_enums = getattr(obj, "_generate_enums", [])  # Extra Keys? Reasons?
    if len(generate_enums):
        for name, suffix in generate_enums:
            if not len(getattr(obj, name)) and suffix == "Keys":
                return class_name(obj.type) + "::<NoExtraKeys>"
            else:
                return "{}::<{}>".format(class_name(obj.type), util.Camelize(obj.name) + suffix)
    return class_name(obj.type)


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


def output_rust(objs, output_fd, options={}):
    """
    Given a tree of objects, output Rust code to the file-like object `output_fd`.

    :param objs: A tree of objects (metrics and pings) as returned from
    `parser.parse_objects`.
    :param output_fd: Writeable file to write the output to.
    :param options: options dictionary, presently unused.
    """

    # Monkeypatch a util.snake_case function for the templates to use
    util.snake_case = lambda value: value.replace('.', '_').replace('-', '_')
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

    if len(objs) == 1 and "pings" in objs:
        template_filename = "rust_pings.jinja2"
    else:
        template_filename = "rust.jinja2"

    # Now for the modules for each category.
    template = util.get_jinja2_template(
        template_filename,
        filters=(
            ("rust", rust_datatypes_filter),
            ("snake_case", util.snake_case),
            ("type_name", type_name),
            ("ctor", ctor),
            ("extra_keys", extra_keys),
        ),
    )

    # The list of all args to CommonMetricData (besides category and name).
    # No particular order is required, but I have these in common_metric_data.rs
    # order just to be organized.
    common_metric_data_args = [
        'name',
        'category',
        'send_in_pings',
        'lifetime',
        'disabled',
        'dynamic_label',
    ]

    output_fd.write(template.render(
        all_objs=objs, common_metric_data_args=common_metric_data_args,
        extra_args=util.extra_args))
    output_fd.write("\n")
