# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import mozunit
import pytest

from mozterm import NullTerminal, Terminal


def test_terminal():
    blessed = pytest.importorskip("blessed")
    term = Terminal()
    assert isinstance(term, blessed.Terminal)

    term = Terminal(disable_styling=True)
    assert isinstance(term, NullTerminal)


def test_null_terminal():
    term = NullTerminal()
    assert term.red("foo") == "foo"
    assert term.red == ""
    assert term.color(1) == ""
    assert term.number_of_colors == 0
    assert term.width == 0
    assert term.height == 0
    assert term.is_a_tty == os.isatty(sys.stdout.fileno())


if __name__ == "__main__":
    mozunit.main()
