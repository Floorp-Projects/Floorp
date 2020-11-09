# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate C# code for metrics.
"""

import enum
import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Union  # noqa

from . import metrics
from . import pings
from . import util


def csharp_datatypes_filter(value: util.JSONType) -> str:
    """
    A Jinja2 filter that renders C# literals.

    Based on Python's JSONEncoder, but overrides:
      - lists to use `new string[] {}` (only strings)
      - dicts to use `new Dictionary<string, string> { ...}` (string, string)
      - sets to use `new HashSet<string>() {}` (only strings)
      - enums to use the like-named C# enum
    """

    class CSharpEncoder(json.JSONEncoder):
        def iterencode(self, value):
            if isinstance(value, list):
                assert all(isinstance(x, str) for x in value)
                yield "new string[] {"
                first = True
                for subvalue in value:
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "}"
            elif isinstance(value, dict):
                yield "new Dictionary<string, string> {"
                first = True
                for key, subvalue in value.items():
                    if not first:
                        yield ", "
                    yield "{"
                    yield from self.iterencode(key)
                    yield ", "
                    yield from self.iterencode(subvalue)
                    yield "}"
                    first = False
                yield "}"
            elif isinstance(value, enum.Enum):
                yield (value.__class__.__name__ + "." + util.Camelize(value.name))
            elif isinstance(value, set):
                yield "new HashSet<string>() {"
                first = True
                for subvalue in sorted(list(value)):
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "}"
            else:
                yield from super().iterencode(value)

    return "".join(CSharpEncoder().iterencode(value))


def type_name(obj: Union[metrics.Metric, pings.Ping]) -> str:
    """
    Returns the C# type to use for a given metric or ping object.
    """
    generate_enums = getattr(obj, "_generate_enums", [])
    if len(generate_enums):
        template_args = []
        for member, suffix in generate_enums:
            if len(getattr(obj, member)):
                template_args.append(util.camelize(obj.name) + suffix)
            else:
                if suffix == "Keys":
                    template_args.append("NoExtraKeys")
                else:
                    template_args.append("No" + suffix)

        return "{}<{}>".format(class_name(obj.type), ", ".join(template_args))

    return class_name(obj.type)


def class_name(obj_type: str) -> str:
    """
    Returns the C# class name for a given metric or ping type.
    """
    if obj_type == "ping":
        return "PingType"
    if obj_type.startswith("labeled_"):
        obj_type = obj_type[8:]
    return util.Camelize(obj_type) + "MetricType"


def output_csharp(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output C# code to `output_dir`.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    :param options: options dictionary, with the following optional keys:

        - `namespace`: The package namespace to declare at the top of the
          generated files. Defaults to `GleanMetrics`.
        - `glean_namespace`: The package namespace of the glean library itself.
          This is where glean objects will be imported from in the generated
          code.
    """
    if options is None:
        options = {}

    template = util.get_jinja2_template(
        "csharp.jinja2",
        filters=(
            ("csharp", csharp_datatypes_filter),
            ("type_name", type_name),
            ("class_name", class_name),
        ),
    )

    namespace = options.get("namespace", "GleanMetrics")
    glean_namespace = options.get("glean_namespace", "Mozilla.Glean")

    for category_key, category_val in objs.items():
        filename = util.Camelize(category_key) + ".cs"
        filepath = output_dir / filename

        with filepath.open("w", encoding="utf-8") as fd:
            fd.write(
                template.render(
                    category_name=category_key,
                    objs=category_val,
                    extra_args=util.extra_args,
                    namespace=namespace,
                    glean_namespace=glean_namespace,
                )
            )
            # Jinja2 squashes the final newline, so we explicitly add it
            fd.write("\n")
