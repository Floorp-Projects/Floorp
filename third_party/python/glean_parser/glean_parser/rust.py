# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate Rust code for metrics.
"""

import enum
import json
from pathlib import Path
from typing import Any, Dict, Optional, Union

from . import metrics
from . import pings
from . import tags
from . import util


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
                yield f'"{value}".into()'
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
        return "LabeledMetric<Labeled{}>".format(class_name(obj.type))
    generate_enums = getattr(obj, "_generate_enums", [])  # Extra Keys? Reasons?
    if len(generate_enums):
        for name, suffix in generate_enums:
            if not len(getattr(obj, name)) and suffix == "Keys":
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
    return "&[" + ", ".join([f'"{key}"' for key in allowed_extra_keys]) + "]"


class Category:
    """
    Data struct holding information about a metric to be used in the template.
    """

    def __init__(
        self,
        name: str,
        objs: Dict[str, Union[metrics.Metric, pings.Ping, tags.Tag]],
        contains_pings: bool,
    ):
        self.name = name
        self.objs = objs
        self.contains_pings = contains_pings


def output_rust(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output Rust code to `output_dir`.

    :param objs: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    :param options: options dictionary, not currently used for Rust
    """

    if options is None:
        options = {}

    template = util.get_jinja2_template(
        "rust.jinja2",
        filters=(
            ("rust", rust_datatypes_filter),
            ("snake_case", util.snake_case),
            ("camelize", util.camelize),
            ("type_name", type_name),
            ("extra_type_name", extra_type_name),
            ("ctor", ctor),
            ("extra_keys", extra_keys),
        ),
    )

    filename = "glean_metrics.rs"
    filepath = output_dir / filename
    categories = []

    for category_key, category_val in objs.items():
        contains_pings = any(
            isinstance(obj, pings.Ping) for obj in category_val.values()
        )

        cat = Category(category_key, category_val, contains_pings)
        categories.append(cat)

    with filepath.open("w", encoding="utf-8") as fd:
        fd.write(
            template.render(
                categories=categories,
                extra_args=util.metric_args,
            )
        )
