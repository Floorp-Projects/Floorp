# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
from collections import defaultdict

from mozbuild.configure.options import Option


class HelpFormatter(object):
    def __init__(self, argv0):
        self.intro = ["Usage: %s [options]" % os.path.basename(argv0)]
        self.options = []

    def add(self, option):
        assert isinstance(option, Option)
        if option.possible_origins == ("implied",):
            # Don't display help if our option can only be implied.
            return
        self.options.append(option)

    def format_options_by_category(self, options_by_category):
        ret = []
        for category, options in sorted(
            options_by_category.items(), key=lambda x: x[0]
        ):
            ret.append("  " + category + ":")
            for option in sorted(options, key=lambda opt: opt.option):
                opt = option.option
                if option.choices:
                    opt += "={%s}" % ",".join(option.choices)
                help = self.format_help(option)
                if len(option.default):
                    if help:
                        help += " "
                    help += "[%s]" % ",".join(option.default)

                if len(opt) > 24 or not help:
                    ret.append("    %s" % opt)
                    if help:
                        ret.append("%s%s" % (" " * 30, help))
                else:
                    ret.append("    %-24s  %s" % (opt, help))
            ret.append("")
        return ret

    RE_FORMAT = re.compile(r"{([^|}]*)\|([^|}]*)}")

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
            return ""

        if option.prefix in ("enable", "with"):
            replacement = r"\1"
        elif option.prefix in ("disable", "without"):
            replacement = r"\2"
        else:
            return option.help

        return self.RE_FORMAT.sub(replacement, option.help)

    def usage(self, out):
        options_by_category = defaultdict(list)
        env_by_category = defaultdict(list)
        for option in self.options:
            target = options_by_category if option.name else env_by_category
            target[option.category].append(option)
        options_formatted = [
            "Options: [defaults in brackets after descriptions]"
        ] + self.format_options_by_category(options_by_category)
        env_formatted = ["Environment variables:"] + self.format_options_by_category(
            env_by_category
        )
        print(
            "\n\n".join(
                "\n".join(t) for t in (self.intro, options_formatted, env_formatted)
            ),
            file=out,
        )
