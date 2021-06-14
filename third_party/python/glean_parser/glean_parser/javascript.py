# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate Javascript code for metrics.
"""

import enum
import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Union, Callable  # noqa

from . import metrics
from . import util


def javascript_datatypes_filter(value: util.JSONType) -> str:
    """
    A Jinja2 filter that renders Javascript literals.
    """

    class JavascriptEncoder(json.JSONEncoder):
        def iterencode(self, value):
            if isinstance(value, enum.Enum):
                yield from super().iterencode(util.camelize(value.name))
            elif isinstance(value, set):
                yield "["
                first = True
                for subvalue in sorted(list(value)):
                    if not first:
                        yield ", "
                    yield from self.iterencode(subvalue)
                    first = False
                yield "]"
            else:
                yield from super().iterencode(value)

    return "".join(JavascriptEncoder().iterencode(value))


def class_name_factory(platform: str) -> Callable[[str], str]:
    """
    Returns a function that receives an obj_type and
    returns the correct class name for that time in the current platform.
    """

    def class_name(obj_type: str) -> str:
        if obj_type == "ping":
            class_name = "PingType"
        else:
            if obj_type.startswith("labeled_"):
                obj_type = obj_type[8:]
            class_name = util.Camelize(obj_type) + "MetricType"

        if platform == "qt":
            return "Glean.Glean._private." + class_name

        return class_name

    return class_name


def import_path(obj_type: str) -> str:
    """
    Returns the import path of the given object inside the @mozilla/glean package.
    """
    if obj_type == "ping":
        import_path = "ping"
    else:
        if obj_type.startswith("labeled_"):
            obj_type = obj_type[8:]
        import_path = "metrics/" + obj_type

    return import_path


def args(obj_type: str) -> Dict[str, object]:
    """
    Returns the list of arguments for each object type.
    """
    if obj_type == "ping":
        return {"common": util.ping_args, "extra": []}

    return {"common": util.common_metric_args, "extra": util.extra_metric_args}


def output(
    lang: str,
    objs: metrics.ObjectTree,
    output_dir: Path,
    options: Optional[Dict[str, Any]] = None,
) -> None:
    """
    Given a tree of objects, output Javascript or Typescript code to `output_dir`.

    :param lang: Either "javascript" or "typescript";
    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    :param options: options dictionary, with the following optional keys:
        - `platform`: Which platform are we building for. Options are `webext` and `qt`.
                      Default is `webext`.
        - `version`: The version of the Glean.js Qt library being used.
                     This option is mandatory when targeting Qt. Note that the version
                     string must only contain the major and minor version i.e. 0.14.

    """

    if options is None:
        options = {}

    platform = options.get("platform", "webext")
    if platform not in ["qt", "webext"]:
        raise ValueError(
            f"Unknown platform: {platform}. Accepted platforms are qt and webext."
        )
    version = options.get("version")
    if platform == "qt" and version is None:
        raise ValueError(
            "'version' option is required when building for the 'qt' platform."
        )

    template = util.get_jinja2_template(
        "javascript.jinja2",
        filters=(
            ("class_name", class_name_factory(platform)),
            ("import_path", import_path),
            ("js", javascript_datatypes_filter),
            ("args", args),
        ),
    )

    for category_key, category_val in objs.items():
        extension = ".js" if lang == "javascript" else ".ts"
        filename = util.camelize(category_key) + extension
        filepath = output_dir / filename

        types = set(
            [
                # This takes care of the regular metric type imports
                # as well as the labeled metric subtype imports,
                # thus the removal of the `labeled_` substring.
                #
                # The actual LabeledMetricType import is conditioned after
                # the `has_labeled_metrics` boolean.
                obj.type if not obj.type.startswith("labeled_") else obj.type[8:]
                for obj in category_val.values()
            ]
        )
        has_labeled_metrics = any(
            getattr(metric, "labeled", False) for metric in category_val.values()
        )
        with filepath.open("w", encoding="utf-8") as fd:
            fd.write(
                template.render(
                    category_name=category_key,
                    objs=category_val,
                    extra_args=util.extra_args,
                    platform=platform,
                    version=version,
                    has_labeled_metrics=has_labeled_metrics,
                    types=types,
                    lang=lang,
                )
            )
            # Jinja2 squashes the final newline, so we explicitly add it
            fd.write("\n")

    if platform == "qt":
        # Explicitly create a qmldir file when building for Qt
        template = util.get_jinja2_template("qmldir.jinja2")
        filepath = output_dir / "qmldir"

        with filepath.open("w", encoding="utf-8") as fd:
            fd.write(template.render(categories=objs.keys(), version=version))
            # Jinja2 squashes the final newline, so we explicitly add it
            fd.write("\n")


def output_javascript(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output Javascript code to `output_dir`.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    :param options: options dictionary, with the following optional keys:

        - `namespace`: The identifier of the global variable to assign to.
                       This will only have and effect for Qt and static web sites.
                       Default is `Glean`.
        - `platform`: Which platform are we building for. Options are `webext` and `qt`.
                      Default is `webext`.
    """

    output("javascript", objs, output_dir, options)


def output_typescript(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output Typescript code to `output_dir`.

    # Note

    The only difference between the typescript and javascript templates,
    currently is the file extension.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    :param options: options dictionary, with the following optional keys:

        - `namespace`: The identifier of the global variable to assign to.
                       This will only have and effect for Qt and static web sites.
                       Default is `Glean`.
        - `platform`: Which platform are we building for. Options are `webext` and `qt`.
                      Default is `webext`.
    """

    output("typescript", objs, output_dir, options)
