# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from textwrap import TextWrapper

from mach.config import TYPE_CLASSES
from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


@CommandProvider
class Settings(object):
    """Interact with settings for mach.

    Currently, we only provide functionality to view what settings are
    available. In the future, this module will be used to modify settings, help
    people create configs via a wizard, etc.
    """
    def __init__(self, context):
        self._settings = context.settings

    @Command('settings', category='devenv',
             description='Show available config settings.')
    @CommandArgument('-l', '--list', dest='short', action='store_true',
                     help='Show settings in a concise list')
    def settings(self, short=None):
        """List available settings."""
        types = {v: k for k, v in TYPE_CLASSES.items()}
        wrapper = TextWrapper(initial_indent='# ', subsequent_indent='# ')
        for i, section in enumerate(sorted(self._settings)):
            if not short:
                print('%s[%s]' % ('' if i == 0 else '\n', section))

            for option in sorted(self._settings[section]._settings):
                meta = self._settings[section].get_meta(option)
                desc = meta['description']

                if short:
                    print('%s.%s -- %s' % (section, option, desc.splitlines()[0]))
                    continue

                if option == '*':
                    option = '<option>'

                if 'choices' in meta:
                    value = "{%s}" % ', '.join(meta['choices'])
                else:
                    value = '<%s>' % types[meta['type_cls']]

                print(wrapper.fill(desc))
                print(';%s=%s' % (option, value))
