# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from argparse import ArgumentParser

from .templates import all_templates


class BaseTryParser(ArgumentParser):
    name = 'try'
    common_arguments = [
        [['--no-push'],
         {'dest': 'push',
          'action': 'store_false',
          'help': 'Do not push to try as a result of running this command (if '
                  'specified this command will only print calculated try '
                  'syntax and selection info).',
          }],
        [['--save'],
         {'default': None,
          'help': 'Save selection for future use with --preset.',
          }],
        [['--preset'],
         {'default': None,
          'help': 'Load a saved selection.',
          }],
        [['--list-presets'],
         {'action': 'store_true',
          'default': False,
          'help': 'List available preset selections.',
          }],
    ]
    arguments = []
    templates = []

    def __init__(self, *args, **kwargs):
        ArgumentParser.__init__(self, *args, **kwargs)

        group = self.add_argument_group("{} arguments".format(self.name))
        for cli, kwargs in self.arguments:
            group.add_argument(*cli, **kwargs)

        group = self.add_argument_group("common arguments")
        for cli, kwargs in self.common_arguments:
            group.add_argument(*cli, **kwargs)

        group = self.add_argument_group("template arguments")
        self.templates = {t: all_templates[t]() for t in self.templates}
        for template in self.templates.values():
            template.add_arguments(group)

    def parse_known_args(self, *args, **kwargs):
        args, remainder = ArgumentParser.parse_known_args(self, *args, **kwargs)

        if self.templates:
            args.templates = {}
            for name, cls in self.templates.iteritems():
                context = cls.context(**vars(args))
                if context is not None:
                    args.templates[name] = context

        return args, remainder
