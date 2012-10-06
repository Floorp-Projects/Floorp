# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals


class_handlers = []

def register_method_handler(cls, method, parser_args, arguments):
    class_handlers.append((cls, method, parser_args, arguments))


def populate_argument_parser(parser):
    for cls, method, parser_args, arguments in class_handlers:
        p = parser.add_parser(*parser_args[0], **parser_args[1])

        for arg in arguments:
            p.add_argument(*arg[0], **arg[1])

        p.set_defaults(cls=cls, method=method)
