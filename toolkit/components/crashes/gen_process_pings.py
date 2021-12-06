# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from geckoprocesstypes import process_types


def main(output):
    output.write("""  let processPings = {""")

    for p in process_types:
        string_name = p.string_name
        if p.string_name == "default":
            string_name = "main"
        if p.string_name == "tab":
            string_name = "content"
        output.write(
            """
    "%(proctype)s": %(crashping)s,"""
            % {
                "proctype": string_name,
                "crashping": "true" if p.crash_ping else "false",
            }
        )
    output.write(
        """
  };
"""
    )
