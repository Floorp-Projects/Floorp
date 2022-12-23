# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import requests

from mozbuild.vendor.host_base import BaseHost


class AngleHost(BaseHost):
    def upstream_commit(self, revision):
        raise Exception("Should not be called")

    def upstream_tag(self, revision):
        data = requests.get("https://omahaproxy.appspot.com/all.json").json()

        for row in data:
            if row["os"] == "win64":
                for version in row["versions"]:
                    if version["channel"] == "beta":
                        branch = "chromium/" + version["true_branch"]

                        if revision != "HEAD" and revision != branch:
                            raise Exception(
                                "Passing a --revision for Angle that is not HEAD "
                                + "or the true branch is not supported."
                            )

                        return (
                            branch,
                            version["current_reldate"],
                        )

        raise Exception("Could not find win64 beta version in the JSON response")

    def upstream_snapshot(self, revision):
        raise Exception("Not supported for Angle")
