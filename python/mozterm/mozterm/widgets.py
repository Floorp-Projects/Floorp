# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from .terminal import Terminal


class BaseWidget(object):
    def __init__(self, terminal=None):
        self.term = terminal or Terminal()
        self.stream = self.term.stream


class Footer(BaseWidget):
    """Handles display of a footer in a terminal."""

    def clear(self):
        """Removes the footer from the current terminal."""
        self.stream.write(self.term.move_x(0))
        self.stream.write(self.term.clear_eol())

    def write(self, parts):
        """Write some output in the footer, accounting for terminal width.

        parts is a list of 2-tuples of (encoding_function, input).
        None means no encoding."""

        # We don't want to write more characters than the current width of the
        # terminal otherwise wrapping may result in weird behavior. We can't
        # simply truncate the line at terminal width characters because a)
        # non-viewable escape characters count towards the limit and b) we
        # don't want to truncate in the middle of an escape sequence because
        # subsequent output would inherit the escape sequence.
        max_width = self.term.width
        written = 0
        write_pieces = []
        for part in parts:
            try:
                func, part = part
                encoded = getattr(self.term, func)(part)
            except ValueError:
                encoded = part

            len_part = len(part)
            len_spaces = len(write_pieces)
            if written + len_part + len_spaces > max_width:
                write_pieces.append(part[0:max_width - written - len_spaces])
                written += len_part
                break

            write_pieces.append(encoded)
            written += len_part

        with self.term.location():
            self.term.move(self.term.height-1, 0)
            self.stream.write(' '.join(write_pieces))
