# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate Javascript code for metrics.
"""

import enum
import json
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Callable

from . import __version__
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
    returns the correct class name for that type in the current platform.
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


def extra_type_name(extra_type: str) -> str:
    """
    Returns the equivalent TypeScript type to an extra type.
    """
    if extra_type == "quantity":
        return "number"

    return extra_type


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


def generate_build_date(date: Optional[str]) -> str:
    """
    Generate the build Date object.
    """

    ts = util.build_date(date)

    data = [
        str(ts.year),
        # In JavaScript the first month of the year in calendars is JANUARY which is 0.
        # In Python it's 1-based
        str(ts.month - 1),
        str(ts.day),
        str(ts.hour),
        str(ts.minute),
        str(ts.second),
    ]
    components = ", ".join(data)

    # DatetimeMetricType takes a `Date` instance.
    return f"new Date({components})"  # noqa


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
        - `with_buildinfo`: If "true" a `gleanBuildInfo.(js|ts)` file is generated.
            Otherwise generation of that file is skipped. Defaults to "false".
        - `build_date`: If set to `0` a static unix epoch time will be used.
                        If set to a ISO8601 datetime string (e.g. `2022-01-03T17:30:00`)
                        it will use that date.
                        Other values will throw an error.
                        If not set it will use the current date & time.
        - `fail_rates`: When `true` it fails when encountering rate metrics.
                        When `false` it will warn and skip rate metrics.
                        Defaults to "true".
    """

    if options is None:
        options = {}

    fail_rates = options.get("fail_rates", "true").lower() == "true"
    fail_rate_level = "ERROR" if fail_rates else "WARN"
    platform = options.get("platform", "webext")
    accepted_platforms = ["qt", "webext", "node"]
    if platform not in accepted_platforms:
        raise ValueError(
            f"Unknown platform: {platform}. Accepted platforms are: {accepted_platforms}."  # noqa
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
            ("extra_type_name", extra_type_name),
            ("import_path", import_path),
            ("js", javascript_datatypes_filter),
            ("args", args),
        ),
    )

    for category_key, category_val in objs.items():
        extension = ".js" if lang == "javascript" else ".ts"
        filename = util.camelize(category_key) + extension
        filepath = output_dir / filename

        # FIXME: Add support for rate (and numerator & denominator) in Glean.js
        todelete = []
        for key, metric in category_val.items():
            if isinstance(metric, metrics.Rate):
                print(
                    f"{fail_rate_level}: Rate metric not supported. Metric: {category_key}.{metric.name}",  # noqa: E501
                    file=sys.stderr,
                )
                todelete.append(key)
            if isinstance(metric, metrics.Denominator):
                print(
                    f"{fail_rate_level}: Rate metric not supported. Dropping numerators. Metric: {category_key}.{metric.name}",  # noqa: E501
                    file=sys.stderr,
                )
                del metric.numerators

        if fail_rates and todelete:
            print("Failed due to previous errors.", file=sys.stderr)
            raise ValueError("Unsupported metric type encountered.")

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
                    parser_version=__version__,
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

    with_buildinfo = options.get("with_buildinfo", "").lower() == "true"
    build_date = options.get("build_date", None)
    if with_buildinfo:
        # Write out the special "build info" file
        template = util.get_jinja2_template(
            "javascript.buildinfo.jinja2",
        )
        # This filename needs to start with "glean" so it can never
        # clash with a metric category
        filename = "gleanBuildInfo" + extension
        filepath = output_dir / filename

        with filepath.open("w", encoding="utf-8") as fd:
            fd.write(
                template.render(
                    parser_version=__version__,
                    platform=platform,
                    build_date=generate_build_date(build_date),
                )
            )
            fd.write("\n")

    if platform == "qt":
        # Explicitly create a qmldir file when building for Qt
        template = util.get_jinja2_template("qmldir.jinja2")
        filepath = output_dir / "qmldir"

        with filepath.open("w", encoding="utf-8") as fd:
            fd.write(
                template.render(
                    parser_version=__version__, categories=objs.keys(), version=version
                )
            )
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
