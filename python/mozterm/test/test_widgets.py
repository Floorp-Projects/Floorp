# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from io import StringIO

import mozunit
import pytest

from mozterm import Terminal
from mozterm.widgets import Footer


@pytest.fixture
def terminal():
    blessed = pytest.importorskip("blessed")

    kind = "xterm-256color"
    try:
        term = Terminal(stream=StringIO(), force_styling=True, kind=kind)
    except blessed.curses.error:
        pytest.skip("terminal '{}' not found".format(kind))

    return term


@pytest.mark.skipif(
    not sys.platform.startswith("win"),
    reason="Only do ANSI Escape Sequence comparisons on Windows.",
)
def test_footer(terminal):
    footer = Footer(terminal=terminal)
    footer.write(
        [
            ("bright_black", "foo"),
            ("green", "bar"),
        ]
    )
    value = terminal.stream.getvalue()
    expected = "\x1b7\x1b[90mfoo\x1b(B\x1b[m \x1b[32mbar\x1b(B\x1b[m\x1b8"
    assert value == expected

    footer.clear()
    value = terminal.stream.getvalue()[len(value) :]
    expected = "\x1b[1G\x1b[K"
    assert value == expected


if __name__ == "__main__":
    mozunit.main()
