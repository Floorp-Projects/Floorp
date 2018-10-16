# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re
from mozbuild.configure.options import Option


class HelpFormatter(object):
    def __init__(self, argv0):
        self.intro = ['Usage: %s [options]' % os.path.basename(argv0)]
        self.options = ['Options: [defaults in brackets after descriptions]']
        self.env = ['Environment variables:']

    def add(self, option):
        assert isinstance(option, Option)

        if option.possible_origins == ('implied',):
            # Don't display help if our option can only be implied.
            return

        # TODO: improve formatting
        target = self.options if option.name else self.env
        opt = option.option
        if option.choices:
            opt += '={%s}' % ','.join(option.choices)
        help = self.format_help(option)
        if len(option.default):
            if help:
                help += ' '
            help += '[%s]' % ','.join(option.default)

        if len(opt) > 24 or not help:
            target.append('  %s' % opt)
            if help:
                target.append('%s%s' % (' ' * 28, help))
        else:
            target.append('  %-24s  %s' % (opt, help))

    RE_FORMAT = re.compile(r'{([^|}]+)\|([^|}]+)}')

    # Return formatted help text for --{enable,disable,with,without}-* options.
    #
    # Format is the following syntax:
    #   {String for --enable or --with|String for --disable or --without}
    #
    # For example, '{Enable|Disable} optimizations' will be formatted to
    # 'Enable optimizations' if the options's prefix is 'enable' or 'with',
    # and formatted to 'Disable optimizations' if the options's prefix is
    # 'disable' or 'without'.
    def format_help(self, option):
        if not option.help:
            return ''

        if option.prefix in ('enable', 'with'):
            replacement = r'\1'
        elif option.prefix in ('disable', 'without'):
            replacement = r'\2'
        else:
            return option.help

        return self.RE_FORMAT.sub(replacement, option.help)

    def usage(self, out):
        print('\n\n'.join('\n'.join(t)
                          for t in (self.intro, self.options, self.env)),
              file=out)
