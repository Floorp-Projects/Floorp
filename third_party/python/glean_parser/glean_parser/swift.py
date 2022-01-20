# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate Swift code for metrics.
"""

import enum
import json
from pathlib import Path
from typing import Any, Dict, Optional, Union

from . import metrics
from . import pings
from . import util

# An (imcomplete) list of reserved keywords in Swift.
# These will be replaced in generated code by their escaped form.
SWIFT_RESERVED_NAMES = ["internal", "typealias"]


def swift_datatypes_filter(value: util.JSONType) -> str:
    """
    A Jinja2 filter that renders Swift literals.

    Based on Python's JSONEncoder, but overrides:
      - dicts to use `[key: value]`
      - sets to use `[...]`
      - enums to use the like-named Swift enum
    """

    class SwiftEncoder(json.JSONEncoder):
        def iterencode(self, value):
            if isinstance(value, dict):
                yield "["
                first = True
                for key, subvalue in value.items():
                    if not first:
                        yield ", "
                    yield from self.iterencode(key)
                    yield ": "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "]"
            elif isinstance(value, enum.Enum):
                yield ("." + util.camelize(value.name))
            elif isinstance(value, set):
                yield "["
                first = True
                for subvalue in sorted(list(value)):
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "]"
            elif value is None:
                yield "nil"
            else:
                yield from super().iterencode(value)

    return "".join(SwiftEncoder().iterencode(value))


def type_name(obj: Union[metrics.Metric, pings.Ping]) -> str:
    """
    Returns the Swift type to use for a given metric or ping object.
    """
    generate_enums = getattr(obj, "_generate_enums", [])
    if len(generate_enums):
        template_args = []
        for member, suffix in generate_enums:
            if len(getattr(obj, member)):
                # Ugly hack to support the newer event extras API
                # along the deprecated API.
                # We need to specify both generic parameters,
                # but only for event metrics.
                if suffix == "Extra":
                    if isinstance(obj, metrics.Event):
                        template_args.append("NoExtraKeys")
                    template_args.append(util.Camelize(obj.name) + suffix)
                else:
                    template_args.append(util.Camelize(obj.name) + suffix)
                    if isinstance(obj, metrics.Event):
                        template_args.append("NoExtras")
            else:
                if suffix == "Keys":
                    template_args.append("NoExtraKeys")
                    template_args.append("NoExtras")
                else:
                    template_args.append("No" + suffix)

        return "{}<{}>".format(class_name(obj.type), ", ".join(template_args))

    return class_name(obj.type)


def extra_type_name(typ: str) -> str:
    """
    Returns the corresponding Kotlin type for event's extra key types.
    """

    if typ == "boolean":
        return "Bool"
    elif typ == "string":
        return "String"
    elif typ == "quantity":
        return "Int32"
    else:
        return "UNSUPPORTED"


def class_name(obj_type: str) -> str:
    """
    Returns the Swift class name for a given metric or ping type.
    """
    if obj_type == "ping":
        return "Ping"
    if obj_type.startswith("labeled_"):
        obj_type = obj_type[8:]
    return util.Camelize(obj_type) + "MetricType"


def variable_name(var: str) -> str:
    """
    Returns a valid Swift variable name, escaping keywords if necessary.
    """
    if var in SWIFT_RESERVED_NAMES:
        return "`" + var + "`"
    else:
        return var


class Category:
    """
    Data struct holding information about a metric to be used in the template.
    """

    name: str
    objs: Dict[str, Union[metrics.Metric, pings.Ping]]
    contains_pings: bool


def output_swift(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output Swift code to `output_dir`.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    :param options: options dictionary, with the following optional keys:
        - namespace: The namespace to generate metrics in
        - glean_namespace: The namespace to import Glean from
        - allow_reserved: When True, this is a Glean-internal build
    """
    if options is None:
        options = {}

    template = util.get_jinja2_template(
        "swift.jinja2",
        filters=(
            ("swift", swift_datatypes_filter),
            ("type_name", type_name),
            ("class_name", class_name),
            ("variable_name", variable_name),
            ("extra_type_name", extra_type_name),
        ),
    )

    namespace = options.get("namespace", "GleanMetrics")
    glean_namespace = options.get("glean_namespace", "Glean")

    filename = "Metrics.swift"
    filepath = output_dir / filename
    categories = []

    for category_key, category_val in objs.items():
        contains_pings = any(
            isinstance(obj, pings.Ping) for obj in category_val.values()
        )

        cat = Category()
        cat.name = category_key
        cat.objs = category_val
        cat.contains_pings = contains_pings

        categories.append(cat)

    with filepath.open("w", encoding="utf-8") as fd:
        fd.write(
            template.render(
                categories=categories,
                extra_args=util.metric_args,
                namespace=namespace,
                glean_namespace=glean_namespace,
                allow_reserved=options.get("allow_reserved", False),
            )
        )
        # Jinja2 squashes the final newline, so we explicitly add it
        fd.write("\n")
