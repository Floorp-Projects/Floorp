# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import subprocess
import tempfile
from argparse import ArgumentParser

from .templates import all_templates


class BaseTryParser(ArgumentParser):
    name = 'try'
    common_arguments = [
        [['-m', '--message'],
         {'const': 'editor',
          'default': '{msg}',
          'nargs': '?',
          'help': 'Use the specified commit message, or create it in your '
                  '$EDITOR if blank. Defaults to computed message.',
          }],
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

    def validate(self, args):
        if args.message == 'editor':
            if 'EDITOR' not in os.environ:
                self.error("must set the $EDITOR environment variable to use blank --message")

            with tempfile.NamedTemporaryFile(mode='r') as fh:
                subprocess.call([os.environ['EDITOR'], fh.name])
                args.message = fh.read().strip()

        if '{msg}' not in args.message:
            args.message = '{}\n\n{}'.format(args.message, '{msg}')

    def parse_known_args(self, *args, **kwargs):
        args, remainder = ArgumentParser.parse_known_args(self, *args, **kwargs)
        self.validate(args)

        if self.templates:
            args.templates = {}
            for name, cls in self.templates.iteritems():
                context = cls.context(**vars(args))
                if context is not None:
                    args.templates[name] = context

        return args, remainder
