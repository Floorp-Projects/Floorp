# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate server Javascript code for collecting events.

This outputter is different from the rest of the outputters in that the code it
generates does not use the Glean SDK. It is meant to be used to collect events
using "events as pings" pattern in server-side environments. In these environments
SDK assumptions to measurement window and connectivity don't hold.
Generated code takes care of assembling pings with metrics, serializing to messages
conforming to Glean schema, and logging with mozlog. Then it's the role of the ingestion
pipeline to pick the messages up and process.

Warning: this outputter supports limited set of metrics,
see `SUPPORTED_METRIC_TYPES` below.
"""
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Optional, List

from . import __version__
from . import metrics
from . import util

# Adding a metric here will require updating the `generate_js_metric_type` function
# and might require changes to the template.
SUPPORTED_METRIC_TYPES = ["string"]


def event_class_name(pingName: str) -> str:
    return util.Camelize(pingName) + "ServerEvent"


def generate_metric_name(metric: metrics.Metric) -> str:
    return f"{metric.category}.{metric.name}"


def generate_metric_argument_name(metric: metrics.Metric) -> str:
    return f"{metric.category}_{metric.name}"


def generate_js_metric_type(metric: metrics.Metric) -> str:
    return metric.type


def generate_metric_argument_description(metric: metrics.Metric) -> str:
    return metric.description.replace("\n", " ").rstrip()


def generate_ping_factory_method(ping: str) -> str:
    return f"create{util.Camelize(ping)}Event"


def output(
    lang: str,
    objs: metrics.ObjectTree,
    output_dir: Path,
) -> None:
    """
    Given a tree of objects, output Javascript or Typescript code to `output_dir`.

    The output is a single file containing all the code for assembling pings with
    metrics, serializing, and submitting.

    :param lang: Either "javascript" or "typescript";
    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    """

    template = util.get_jinja2_template(
        "javascript_server.jinja2",
        filters=(
            ("event_class_name", event_class_name),
            ("metric_name", generate_metric_name),
            ("metric_argument_name", generate_metric_argument_name),
            ("js_metric_type", generate_js_metric_type),
            ("metric_argument_description", generate_metric_argument_description),
            ("factory_method", generate_ping_factory_method),
        ),
    )

    # In this environment we don't use a concept of measurement window for collecting
    # metrics. Only "events as pings" are supported.
    # For each ping we generate code which contains all the logic for assembling it
    # with metrics, serializing, and submitting. Therefore we don't generate classes for
    # each metric as in standard outputters.
    PING_METRIC_ERROR_MSG = (
        " Server-side environment is simplified and this"
        + " parser doesn't generate individual metric files. Make sure to pass all"
        + " your ping and metric definitions in a single invocation of the parser."
    )
    if "pings" not in objs:
        print("❌ No ping definition found." + PING_METRIC_ERROR_MSG)
        return

    # Go through all metrics in objs and build a map of
    # ping->list of metric categories->list of metrics
    # for easier processing in the template.
    ping_to_metrics: Dict[str, Dict[str, List[metrics.Metric]]] = defaultdict(dict)
    for _category_key, category_val in objs.items():
        for _metric_name, metric in category_val.items():
            if isinstance(metric, metrics.Metric):
                if metric.type not in SUPPORTED_METRIC_TYPES:
                    print(
                        "❌ Ignoring unsupported metric type: "
                        + f"{metric.type}:{metric.name}."
                        + " Reach out to Glean team to add support for this"
                        + " metric type."
                    )
                    continue
                for ping in metric.send_in_pings:
                    metrics_by_type = ping_to_metrics[ping]
                    metrics_list = metrics_by_type.setdefault(metric.type, [])
                    metrics_list.append(metric)

    if not ping_to_metrics:
        print("❌ No pings with metrics found." + PING_METRIC_ERROR_MSG)
        return

    extension = ".js" if lang == "javascript" else ".ts"
    filepath = output_dir / ("server_events" + extension)
    with filepath.open("w", encoding="utf-8") as fd:
        fd.write(
            template.render(
                parser_version=__version__,
                pings=ping_to_metrics,
                lang=lang,
            )
        )


def output_javascript(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output Javascript code to `output_dir`.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    """

    output("javascript", objs, output_dir)


def output_typescript(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]] = None
) -> None:
    """
    Given a tree of objects, output Typescript code to `output_dir`.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    """

    output("typescript", objs, output_dir)
