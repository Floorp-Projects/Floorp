# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from jinja2 import Environment, FileSystemLoader, StrictUndefined
import os


def main(output_fd, input_filename, *args):
    # FileSystemLoader requires the path to the directory containing templates,
    # not the file name of the template itself. We hang onto the leaf name
    # which will shortly be passed to Environment.get_template.
    (path, leaf) = os.path.split(input_filename)

    # Jinja's default value for undefined is too permissive and would allow
    # omissions to slip into the generated output. We set undefined to
    # StrictUndefined to force Jinja to raise an exception any time a required
    # value is missing.
    env = Environment(
        loader=FileSystemLoader(path, encoding="utf-8"),
        autoescape=True,
        undefined=StrictUndefined,
    )
    tpl = env.get_template(leaf)

    context = dict()

    # args should all be key=value pairs that will be added to the context.
    # Note that all values are *strings*, so the Jinja template may need to
    # convert them to other types during processing.
    # (As in Python, the empty string is falsy, so simple boolean checks are possible)
    for arg in args:
        (k, v) = arg.split("=", 1)
        context[k] = v

    # Now run the template and send its output directly to output_fd
    tpl.stream(context).dump(output_fd, encoding="utf-8")
