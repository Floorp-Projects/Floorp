# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import sys

REVISION_MATCHER = re.compile(r"remote:.*/try/rev/([\w]*)[ \t]*$")


class LogProcessor:
    def __init__(self):
        self.buf = ""
        self.stdout = sys.__stdout__
        self._revision = None

    @property
    def revision(self):
        return self._revision

    def write(self, buf):
        while buf:
            try:
                newline_index = buf.index("\n")
            except ValueError:
                # No newline, wait for next call
                self.buf += buf
                break

            # Get data up to next newline and combine with previously buffered data
            data = self.buf + buf[: newline_index + 1]
            buf = buf[newline_index + 1 :]

            # Reset buffer then output line
            self.buf = ""
            if data.strip() == "":
                continue
            self.stdout.write(data.strip("\n") + "\n")

            # Check if a temporary commit wa created
            match = REVISION_MATCHER.match(data)
            if match:
                # Last line found is the revision we want
                self._revision = match.group(1)
