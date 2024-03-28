#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

# This script is used by updatebot and 'mach vendor' to find the latest
# application-services nightly version and update the pin in
# ApplicationServices.kt.

from urllib.parse import quote_plus

import requests
from mozbuild.vendor.host_base import BaseHost


def taskcluster_indexed_artifact_url(index_name, artifact_path):
    artifact_path = quote_plus(artifact_path)
    return (
        "https://firefox-ci-tc.services.mozilla.com/"
        f"api/index/v1/task/{index_name}/artifacts/{artifact_path}"
    )


def get_as_nightly_json(version="latest"):
    r = requests.get(
        taskcluster_indexed_artifact_url(
            f"project.application-services.v2.nightly.{version}",
            "public/build/nightly.json",
        )
    )
    r.raise_for_status()
    return r.json()


class ASHost(BaseHost):
    def upstream_tag(self, revision):
        if revision == "HEAD":
            index = "latest"
        else:
            index = revision
        json = get_as_nightly_json(index)
        return json["version"], json["commit"]
