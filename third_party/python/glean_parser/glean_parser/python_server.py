# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate server Python code for collecting events.

This outputter is different from the rest of the outputters in that the code it
generates does not use the Glean SDK. It is meant to be used to collect events
in server-side environments. In these environments SDK assumptions to measurement
window and connectivity don't hold.
Generated code takes care of assembling pings with metrics, and serializing to messages
conforming to Glean schema.

Warning: this outputter supports limited set of metrics,
see `SUPPORTED_METRIC_TYPES` below.

The generated code creates a `ServerEventLogger` class for each ping that has
at least one event metric. The class has a `record` method for each event metric.
"""

from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Optional, List

from . import __version__
from . import metrics
from . import util

# Adding a metric here will require updating the `generate_metric_type` function
# and require adjustments to `metrics` variables the the template.
SUPPORTED_METRIC_TYPES = ["string", "quantity", "event"]


def camelize(s: str) -> str:
    return util.Camelize(s)


def generate_metric_type(metric_type: str) -> str:
    if metric_type == "quantity":
        return "int"
    elif metric_type == "string":
        return "str"
    elif metric_type == "boolean":
        return "bool"
    else:
        print("❌ Unable to generate Python type from metric type: " + metric_type)
        exit
        return "NONE"


def clean_string(s: str) -> str:
    return s.replace("\n", " ").rstrip()


def generate_ping_factory_method(ping: str) -> str:
    return f"create_{util.snake_case(ping)}_server_event_logger"


def generate_event_record_function_name(event_metric: metrics.Metric) -> str:
    return (
        f"record_{util.snake_case(event_metric.category)}_"
        + f"{util.snake_case(event_metric.name)}"
    )


def output_python(
    objs: metrics.ObjectTree, output_dir: Path, options: Optional[Dict[str, Any]]
) -> None:
    """
    Given a tree of objects, output Python code to `output_dir`.

    The output is a file containing all the code for assembling pings with
    metrics, serializing, and submitting, and an empty `__init__.py` file to
    make the directory a package.

    :param objects: A tree of objects (metrics and pings) as returned from
        `parser.parse_objects`.
    :param output_dir: Path to an output directory to write to.
    """

    template = util.get_jinja2_template(
        "python_server.jinja2",
        filters=(
            ("camelize", camelize),
            ("py_metric_type", generate_metric_type),
            ("clean_string", clean_string),
            ("factory_method", generate_ping_factory_method),
            ("record_event_function_name", generate_event_record_function_name),
        ),
    )

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

    for ping, metrics_by_type in ping_to_metrics.items():
        if "event" not in metrics_by_type:
            print(
                f"❌ No event metrics found for ping: {ping}."
                + " At least one event metric is required."
            )
            return

    extension = ".py"
    filepath = output_dir / ("server_events" + extension)
    with filepath.open("w", encoding="utf-8") as fd:
        fd.write(template.render(parser_version=__version__, pings=ping_to_metrics))

    # create an empty `__init__.py` file to make the directory a package
    init_file = output_dir / "__init__.py"
    with init_file.open("w", encoding="utf-8") as fd:
        fd.write("")
