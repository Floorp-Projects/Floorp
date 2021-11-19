# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from geckoprocesstypes import process_types


def main(output):
    output.write(
        """
  processTypes: {"""
    )

    for p in process_types:
        string_name = p.string_name
        if p.string_name == "default":
            string_name = "main"
        output.write(
            """
    // A crash in the %(procname)s process.
    %(proctype)d: "%(procname)s","""
            % {
                "proctype": p.enum_value,
                "procname": string_name,
            }
        )
    output.write(
        """
  },"""
    )
