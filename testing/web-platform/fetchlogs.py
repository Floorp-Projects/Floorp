# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import cStringIO
import gzip
import json
import os
import requests
import urlparse

treeherder_base = "https://treeherder.mozilla.org/"

"""Simple script for downloading structured logs from treeherder.

For the moment this is specialised to work with web-platform-tests
logs; in due course it should move somewhere generic and get hooked
up to mach or similar"""

# Interpretation of the "job" list from
# https://github.com/mozilla/treeherder-service/blob/master/treeherder/webapp/api/utils.py#L18

def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("branch", action="store",
                        help="Branch on which jobs ran")
    parser.add_argument("commit",
                        action="store",
                        help="Commit hash for push")

    return parser

def download(url, prefix, dest, force_suffix=True):
    if dest is None:
        dest = "."

    if prefix and not force_suffix:
        name = os.path.join(dest, prefix + ".log")
    else:
        name = None
    counter = 0

    while not name or os.path.exists(name):
        counter += 1
        sep = "" if not prefix else "-"
        name = os.path.join(dest, prefix + sep + str(counter) + ".log")

    with open(name, "wb") as f:
        resp = requests.get(url)
        f.write(resp.text.encode(resp.encoding))

def get_blobber_url(branch, job):
    job_id = job[8]
    resp = requests.get(urlparse.urljoin(treeherder_base,
                                         "/api/project/%s/artifact/?job_id=%i&name=Job%%20Info" % (branch,
                                                                                                   job_id)))
    job_data = resp.json()
    print job_data
    if job_data:
        assert len(job_data) == 1
        job_data = job_data[0]
        try:
            details = job_data["blob"]["job_details"]
            for item in details:
                if item["value"] == "wpt_structured_full.log":
                    return item["url"]
        except:
            return None


def get_structured_logs(branch, commit, dest=None):
    resp = requests.get(urlparse.urljoin(treeherder_base, "/api/project/%s/resultset/?revision=%s" % (branch,
                                                                                                      commit)))
    job_data = resp.json()

    for result in job_data["results"]:
        for platform in result["platforms"]:
            for group in platform["groups"]:
                for job in group["jobs"]:
                    job_type_name = job[13]
                    if job_type_name.startswith("W3C Web Platform") or job_type_name == "unknown":
                        url = get_blobber_url(branch, job)
                        if url:
                            prefix = job[14] # platform
                            download(url, prefix, None)

def main():
    parser = create_parser()
    args = parser.parse_args()

    get_structured_logs(args.branch, args.commit)

if __name__ == "__main__":
    main()
