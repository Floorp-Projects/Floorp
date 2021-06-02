# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate C++ code for metrics.
"""

import jinja2
import json

from util import generate_metric_ids, generate_ping_ids
from glean_parser import util


def cpp_datatypes_filter(value):
    """
    A Jinja2 filter that renders C++ literals.

    Based on Python's JSONEncoder, but overrides:
      - lists to array literals {}
      - strings to "value"
    """

    class CppEncoder(json.JSONEncoder):
        def iterencode(self, value):
            if isinstance(value, list):
                yield "{"
                first = True
                for subvalue in list(value):
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "}"
            elif isinstance(value, str):
                yield '"' + value + '"'
            else:
                yield from super().iterencode(value)

    return "".join(CppEncoder().iterencode(value))


def type_name(obj):
    """
    Returns the C++ type to use for a given metric object.
    """

    if getattr(obj, "labeled", False):
        class_name = util.Camelize(obj.type[8:])  # strips "labeled_" off the front.
        return "Labeled<impl::{}Metric>".format(class_name)
    generate_enums = getattr(obj, "_generate_enums", [])  # Extra Keys? Reasons?
    if len(generate_enums):
        for name, suffix in generate_enums:
            if not len(getattr(obj, name)) and suffix == "Keys":
                return util.Camelize(obj.type) + "Metric<NoExtraKeys>"
            else:
                # we always use the `extra` suffix,
                # because we only expose the new event API
                suffix = "Extra"
                return "{}Metric<{}>".format(
                    util.Camelize(obj.type), util.Camelize(obj.name) + suffix
                )
    return util.Camelize(obj.type) + "Metric"


def extra_type_name(typ: str) -> str:
    """
    Returns the corresponding Rust type for event's extra key types.
    """

    if typ == "boolean":
        return "bool"
    elif typ == "string":
        return "nsCString"
    elif typ == "quantity":
        return "uint32_t"
    else:
        return "UNSUPPORTED"


def output_cpp(objs, output_fd, options={}):
    """
    Given a tree of objects, output C++ code to the file-like object `output_fd`.

    :param objs: A tree of objects (metrics and pings) as returned from
    `parser.parse_objects`.
    :param output_fd: Writeable file to write the output to.
    :param options: options dictionary.
    """

    # Monkeypatch a util.snake_case function for the templates to use
    util.snake_case = lambda value: value.replace(".", "_").replace("-", "_")
    # Monkeypatch util.get_jinja2_template to find templates nearby

    def get_local_template(template_name, filters=()):
        env = jinja2.Environment(
            loader=jinja2.PackageLoader("cpp", "templates"),
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

    if len(objs) == 1 and "pings" in objs:
        template_filename = "cpp_pings.jinja2"
    else:
        template_filename = "cpp.jinja2"

    template = util.get_jinja2_template(
        template_filename,
        filters=(
            ("cpp", cpp_datatypes_filter),
            ("snake_case", util.snake_case),
            ("type_name", type_name),
            ("extra_type_name", extra_type_name),
            ("metric_id", get_metric_id),
            ("ping_id", get_ping_id),
            ("Camelize", util.Camelize),
        ),
    )

    output_fd.write(template.render(all_objs=objs))
    output_fd.write("\n")
