# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import re

SHELL_QUOTE_RE = re.compile(r"[\\\t\r\n \'\"#<>&|`(){}$;\*\?]")


def _quote(s):
    """Given a string, returns a version that can be used literally on a shell
    command line, enclosing it with single quotes if necessary.

    As a special case, if given an int, returns a string containing the int,
    not enclosed in quotes.
    """
    if type(s) == int:
        return "%d" % s

    # Empty strings need to be quoted to have any significance
    if s and not SHELL_QUOTE_RE.search(s) and not s.startswith("~"):
        return s

    # Single quoted strings can contain any characters unescaped except the
    # single quote itself, which can't even be escaped, so the string needs to
    # be closed, an escaped single quote added, and reopened.
    t = type(s)
    return t("'%s'") % s.replace(t("'"), t("'\\''"))


def quote(*strings):
    """Given one or more strings, returns a quoted string that can be used
    literally on a shell command line.

        >>> quote('a', 'b')
        "a b"
        >>> quote('a b', 'c')
        "'a b' c"
    """
    return " ".join(_quote(s) for s in strings)
