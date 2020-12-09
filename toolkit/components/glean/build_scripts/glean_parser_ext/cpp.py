# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate C++ code for metrics.
"""

import jinja2

from util import generate_metric_ids, is_implemented_metric_type
from glean_parser import util


def type_name(obj):
    """
    Returns the C++ type to use for a given metric object.
    """

    return util.Camelize(obj.type) + "Metric"


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
        for filter_name, filter_func in filters:
            env.filters[filter_name] = filter_func
        return env.get_template(template_name)

    util.get_jinja2_template = get_local_template
    get_metric_id = generate_metric_ids(objs)

    template_filename = "cpp.jinja2"
    template = util.get_jinja2_template(
        template_filename,
        filters=(
            ("snake_case", util.snake_case),
            ("type_name", type_name),
            ("metric_id", get_metric_id),
            ("is_implemented_type", is_implemented_metric_type),
        ),
    )

    output_fd.write(template.render(all_objs=objs))
    output_fd.write("\n")
