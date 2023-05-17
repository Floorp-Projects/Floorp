#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""
A script to help generate telemetry renewal csv and request template.
This script also modifies metrics.yaml to mark soon to expired telemetry entries.
"""

import csv
import json
import os
import sys

import yaml
from yaml.loader import FullLoader

METRICS_FILENAME = "../app/metrics.yaml"
NEW_METRICS_FILENAME = "../app/metrics_new.yaml"
GLEAN_DICTIONARY_PREFIX = "https://dictionary.telemetry.mozilla.org/apps/fenix/metrics/"

# This is to make sure we only write headers for the csv file once
write_header = True
# The number of soon to expired telemetry detected
total_count = 0

USAGE = """usage: ./{script_name} future_fenix_version_number"""

# list of values that we care about
_KEY_FILTER = [
    "type",
    "description",
    "bugs",
    "data_reviews",
    "expires",
]


def response(last_key, content, expire_version, writer, renewal):
    global write_header
    global total_count
    for key, value in content.items():
        if (key == "$schema") or (key == "no_lint"):
            continue
        if key == "disabled":
            continue

        if ("expires" in value) and (
            (value["expires"] == "never") or (not value["expires"] <= expire_version)
        ):
            continue

        if key == "type":
            remove_keys = []
            for key in content.keys():
                if key not in _KEY_FILTER:
                    remove_keys.append(key)

            for key in remove_keys:
                content.pop(key)

            content["bugs"] = content["bugs"][0]
            content["data_reviews"] = content["data_reviews"][0]
            total_count += 1

            # name of the telemtry
            dictionary_url = GLEAN_DICTIONARY_PREFIX + last_key.lstrip(".").replace(".", "_")
            result = {"#": total_count, "name": last_key.lstrip("."), "glean dictionary": dictionary_url}
            result.update(content)

            # add columns for product to fille out, these should always be added at the end
            result.update({"keep(Y/N)": ""})
            result.update({"new expiry version": ""})
            result.update({"reason to extend": ""})

            # output data-renewal request template
            if write_header:
                header = result.keys()
                writer.writerow(header)
                write_header = False
                renewal.write("# Request for Data Collection Renewal\n")
                renewal.write("### Renew for 1 year\n")
                renewal.write("Total: TBD\n")
                renewal.write("———\n")

            writer.writerow(result.values())

            renewal.write("`" + last_key.lstrip(".") + "`:\n")
            renewal.write(
                "1) Provide a link to the initial Data Collection Review Request for this collection.\n"
            )
            renewal.write("    - " + content["data_reviews"] + "\n")
            renewal.write("\n")
            renewal.write("2) When will this collection now expire?\n")
            renewal.write("    - TBD\n")
            renewal.write("\n")
            renewal.write("3) Why was the initial period of collection insufficient?\n")
            renewal.write("    - TBD\n")
            renewal.write("\n")
            renewal.write("———\n")
            return

        if type(value) is dict:
            response(last_key + "." + key, value, expire_version, writer, renewal)


with open(METRICS_FILENAME, "r") as f:
    try:
        arg1 = sys.argv[1]
    except:
        print("usage is to include argument of the form `100`")
        quit()

    # parse metrics.yaml to json
    write_header = True
    data = yaml.load(f, Loader=FullLoader)
    json_data = json.dumps(data)
    content = json.loads(str(json_data))
    csv_filename = arg1 + "_expiry_list.csv"
    renewal_filename = arg1 + "_renewal_request.txt"
    current_version = int(arg1)

    # remove files created by last run if exists
    if os.path.exists(csv_filename):
        print("remove old csv file")
        os.remove(csv_filename)

    # remove files created by last run if exists
    if os.path.exists(renewal_filename):
        print("remove old renewal request template file")
        os.remove(renewal_filename)

    # remove files created by last run if exists
    if os.path.exists(NEW_METRICS_FILENAME):
        print("remove old metrics yaml file")
        os.remove(NEW_METRICS_FILENAME)

    data_file = open(csv_filename, "w")
    csv_writer = csv.writer(data_file)
    renewal_file = open(renewal_filename, "w")

    response("", content, current_version, csv_writer, renewal_file)
    renewal_file.close()
    print("Completed")
    print("Total count: " + str(total_count))

    # Go through the metrics.yaml file to mark expired telemetry
    verify_count = 0
    f.seek(0, 0)
    data = f.readlines()
    with open(NEW_METRICS_FILENAME, "w") as f2:
        for line in data:
            if line.lstrip(" ").startswith("expires: ") and not (
                line.lstrip(" ").startswith("expires: never")
            ):
                start_pos = len("expires: ")
                version = int(line.lstrip(" ")[start_pos:])
                if version <= current_version:
                    verify_count += 1
                    f2.writelines(
                        line.rstrip("\n")
                        + " /* TODO <"
                        + str(verify_count)
                        + "> require renewal */\n"
                    )
                else:
                    f2.writelines(line)
            else:
                f2.writelines(line)
        f2.close()

        print("\n==============================")
        if total_count != verify_count:
            print("!!! Count check failed !!!")
        else:
            print("Count check passed")
        print("==============================")

        os.remove(METRICS_FILENAME)
        os.rename(NEW_METRICS_FILENAME, METRICS_FILENAME)
