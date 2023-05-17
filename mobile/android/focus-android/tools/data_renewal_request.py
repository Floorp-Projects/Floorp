#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""
A script to help generate a data review request comment. Once the CSV has been filled by product,
copy the filled version into the tools directory and run this script from there to generate a filled
renewal request data review comment.
"""

import csv
import sys

try:
    version = sys.argv[1]
except:
    print("usage is to include arguments of the form <version>")
    quit()

expiry_filename = version + "_expiry_list.csv"
filled_renewal_filename = version + "_filled_renewal_request.txt"

csv_reader = csv.DictReader(open(expiry_filename, "r"))
output_string = ""
total_count = 0
updated_version = int(version) + 13
for row in csv_reader:
    if row["keep(Y/N)"] == "n":
        continue
    total_count += 1
    output_string += f'` {row["name"]}`\n'
    output_string += "1) Provide a link to the initial Data Collection Review Request for this collection.\n"
    output_string += f'    - {eval(row["data_reviews"])[0]}\n'
    output_string += "\n"
    output_string += "2) When will this collection now expire?\n"
    if len(row["new expiry version"]) == 0:
        output_string += f"    - {updated_version}\n"
    else:
        output_string += f'    - {row["new expiry version"]}\n'

    output_string += "\n"
    output_string += "3) Why was the initial period of collection insufficient?\n"
    output_string += f'    - {row["reason to extend"]}\n'
    output_string += "\n"
    output_string += "———\n"

header = "# Request for Data Collection Renewal\n"
header += "### Renew for 1 year\n"
header += f"Total: {total_count}\n"
header += "———\n\n"

with open(filled_renewal_filename, "w+") as out:
    out.write(header + output_string)
    out.close()
