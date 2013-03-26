# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import collections


class MachRegistrar(object):
    """Container for mach command and config providers."""

    def __init__(self):
        self.command_handlers = {}
        self.settings_providers = set()

    def register_command_handler(self, handler):
        name = handler.parser_args[0][0]

        self.command_handlers[name] = handler

    def register_settings_provider(self, cls):
        self.settings_providers.add(cls)

    def populate_argument_parser(self, parser):
        for command in sorted(self.command_handlers.keys()):
            handler = self.command_handlers[command]
            handler.parser = parser.add_parser(*handler.parser_args[0],
                **handler.parser_args[1])

            for arg in handler.arguments:
                handler.parser.add_argument(*arg[0], **arg[1])

            handler.parser.set_defaults(mach_class=handler.cls,
                mach_method=handler.method,
                mach_pass_context=handler.pass_context)


Registrar = MachRegistrar()

