#!/usr/bin/env python
from __future__ import absolute_import, print_function

import site
import os
import logging
import argparse
import json

site.addsitedir("/home/worker/tools/lib/python")

from balrog.submitter.cli import ReleaseSubmitterV4
from util.retry import retry

log = logging.getLogger(__name__)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True)
    parser.add_argument("-a", "--api-root", required=True,
                        help="Balrog API root")
    parser.add_argument("-v", "--verbose", action="store_const",
                        dest="loglevel", const=logging.DEBUG,
                        default=logging.INFO)
    parser.add_argument("--product", help="Override product name from application.ini")
    args = parser.parse_args()
    logging.basicConfig(format="%(asctime)s - %(levelname)s - %(message)s",
                        level=args.loglevel)
    logging.getLogger("requests").setLevel(logging.WARNING)
    logging.getLogger("boto").setLevel(logging.WARNING)

    balrog_username = os.environ.get("BALROG_USERNAME")
    balrog_password = os.environ.get("BALROG_PASSWORD")
    suffix = os.environ.get("BALROG_BLOB_SUFFIX")
    if not balrog_username and not balrog_password:
        raise RuntimeError("BALROG_USERNAME and BALROG_PASSWORD environment "
                           "variables should be set")
    if not suffix:
        raise RuntimeError("BALROG_BLOB_SUFFIX environment variable should be set")

    manifest = json.load(open(args.manifest))
    auth = (balrog_username, balrog_password)

    for e in manifest:
        complete_info = [{
            "hash": e["hash"],
            "size": e["size"],
        }]

        submitter = ReleaseSubmitterV4(api_root=args.api_root, auth=auth,
                                       suffix=suffix)
        productName = args.product or e["appName"]
        retry(lambda: submitter.run(
            platform=e["platform"], productName=productName,
            version=e["toVersion"],
            build_number=e["toBuildNumber"],
            appVersion=e["version"], extVersion=e["version"],
            buildID=e["to_buildid"], locale=e["locale"],
            hashFunction='sha512', completeInfo=complete_info),
            attempts=30, sleeptime=10, max_sleeptime=60, jitter=3,
        )


if __name__ == '__main__':
    main()
