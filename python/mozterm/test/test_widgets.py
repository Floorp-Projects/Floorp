# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from io import StringIO

import mozunit
import pytest

from mozterm import Terminal
from mozterm.widgets import Footer


@pytest.fixture
def terminal(monkeypatch):
    blessings = pytest.importorskip('blessings')

    kind = 'xterm-256color'
    try:
        term = Terminal(stream=StringIO(), force_styling=True, kind=kind)
    except blessings.curses.error:
        pytest.skip("terminal '{}' not found".format(kind))

    # For some reason blessings returns None for width/height though a comment
    # says that shouldn't ever happen.
    monkeypatch.setattr(term, '_height_and_width', lambda: (100, 100))
    return term


def test_footer(terminal):
    footer = Footer(terminal=terminal)
    footer.write([
        ('dim', 'foo'),
        ('green', 'bar'),
    ])
    value = terminal.stream.getvalue()
    expected = "\x1b7\x1b[2mfoo\x1b(B\x1b[m \x1b[32mbar\x1b(B\x1b[m\x1b8"
    assert value == expected

    footer.clear()
    value = terminal.stream.getvalue()[len(value):]
    expected = "\x1b[1G\x1b[K"
    assert value == expected


if __name__ == '__main__':
    mozunit.main()
