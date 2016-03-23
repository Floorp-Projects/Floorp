# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
from textwrap import TextWrapper

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

POLIB_NOT_FOUND = """
Could not detect the 'polib' package on the local system.
Please run:

    pip install polib
""".lstrip()


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
        if short:
            for section in sorted(self._settings):
                for option in sorted(self._settings[section]._settings):
                    short, full = self._settings.option_help(section, option)
                    print('%s.%s -- %s' % (section, option, short))
            return

        wrapper = TextWrapper(initial_indent='# ', subsequent_indent='# ')
        for section in sorted(self._settings):
            print('[%s]' % section)

            for option in sorted(self._settings[section]._settings):
                short, full = self._settings.option_help(section, option)
                print(wrapper.fill(full))

                if option != '*':
                    print(';%s =' % option)
                    print('')

    @SubCommand('settings', 'locale-gen',
                description='Generate or update gettext .po and .mo locale files.')
    @CommandArgument('sections', nargs='*',
                     help='A list of strings in the form of either <section> or '
                          '<section>.<option> to translate. By default, prompt to '
                          'translate all applicable options.')
    @CommandArgument('--locale', default='en_US',
                     help='Locale to generate, defaults to en_US.')
    @CommandArgument('--overwrite', action='store_true', default=False,
                     help='Overwrite pre-existing entries in .po files.')
    def locale_gen(self, sections, **kwargs):
        try:
            import polib
        except ImportError:
            print(POLIB_NOT_FOUND)
            return 1

        self.was_prompted = False

        sections = sections or self._settings
        for section in sections:
            self._gen_section(section, **kwargs)

        if not self.was_prompted:
            print("All specified options already have an {} translation. "
                  "To overwrite existing translations, pass --overwrite."
                  .format(kwargs['locale']))

    def _gen_section(self, section, **kwargs):
        if '.' in section:
            section, option = section.split('.')
            return self._gen_option(section, option, **kwargs)

        for option in sorted(self._settings[section]._settings):
            self._gen_option(section, option, **kwargs)

    def _gen_option(self, section, option, locale, overwrite):
        import polib

        meta = self._settings[section]._settings[option]

        localedir = os.path.join(meta['localedir'], locale, 'LC_MESSAGES')
        if not os.path.isdir(localedir):
            os.makedirs(localedir)

        path = os.path.join(localedir, '{}.po'.format(section))
        if os.path.isfile(path):
            po = polib.pofile(path)
        else:
            po = polib.POFile()

        optionid = '{}.{}'.format(section, option)
        for name in ('short', 'full'):
            msgid = '{}.{}'.format(optionid, name)
            entry = po.find(msgid)
            if not entry:
                entry = polib.POEntry(msgid=msgid)
                po.append(entry)

            if entry in po.translated_entries() and not overwrite:
                continue

            self.was_prompted = True

            msgstr = raw_input("Translate {} to {}:\n"
                               .format(msgid, locale))
            entry.msgstr = msgstr

        if self.was_prompted:
            mopath = os.path.join(localedir, '{}.mo'.format(section))
            po.save(path)
            po.save_as_mofile(mopath)
