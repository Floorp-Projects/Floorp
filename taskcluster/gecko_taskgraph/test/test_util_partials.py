# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from unittest import mock

from mozunit import main

from gecko_taskgraph.util import partials

release_blob = {
    "fileUrls": {
        "release-localtest": {
            "completes": {
                "*": "%OS_FTP%/%LOCALE%/firefox-92.0.1.complete.mar",
            }
        }
    },
    "platforms": {
        "WINNT_x86_64-msvc": {
            "locales": {
                "en-US": {
                    "buildID": "20210922161155",
                }
            }
        }
    },
}


def nightly_blob(release):
    return {
        "platforms": {
            "WINNT_x86_64-msvc": {
                "locales": {
                    "en-US": {
                        "buildID": release[-14:],
                        "completes": [{"fileUrl": release}],
                    }
                }
            }
        }
    }


class TestReleaseHistory(unittest.TestCase):
    @mock.patch("gecko_taskgraph.util.partials.get_release_builds")
    @mock.patch("gecko_taskgraph.util.partials.get_sorted_releases")
    def test_populate_release_history(self, get_sorted_releases, get_release_builds):
        self.assertEqual(
            partials.populate_release_history(
                "Firefox", "mozilla-release", partial_updates={}
            ),
            {},
        )
        get_sorted_releases.assert_not_called()
        get_release_builds.assert_not_called()

        def patched_get_sorted_releases(product, branch):
            assert branch == "mozilla-central"
            return [
                "Firefox-mozilla-central-nightly-20211003201113",
                "Firefox-mozilla-central-nightly-20211003100640",
                "Firefox-mozilla-central-nightly-20211002213629",
                "Firefox-mozilla-central-nightly-20211002095048",
                "Firefox-mozilla-central-nightly-20211001214601",
                "Firefox-mozilla-central-nightly-20211001093323",
            ]

        def patched_get_release_builds(release, branch):
            if branch == "mozilla-central":
                return nightly_blob(release)
            if branch == "mozilla-release":
                return release_blob

        get_sorted_releases.side_effect = patched_get_sorted_releases
        get_release_builds.side_effect = patched_get_release_builds

        self.assertEqual(
            partials.populate_release_history(
                "Firefox",
                "mozilla-release",
                partial_updates={"92.0.1": {"buildNumber": 1}},
            ),
            {
                "WINNT_x86_64-msvc": {
                    "en-US": {
                        "target-92.0.1.partial.mar": {
                            "buildid": "20210922161155",
                            "mar_url": "win64/en-US/firefox-92.0.1.complete.mar",
                            "previousVersion": "92.0.1",
                            "previousBuildNumber": 1,
                            "product": "Firefox",
                        }
                    }
                }
            },
        )
        self.assertEqual(
            partials.populate_release_history("Firefox", "mozilla-central"),
            {
                "WINNT_x86_64-msvc": {
                    "en-US": {
                        "target.partial-1.mar": {
                            "buildid": "20211003201113",
                            "mar_url": "Firefox-mozilla-central-nightly-20211003201113",
                        },
                        "target.partial-2.mar": {
                            "buildid": "20211003100640",
                            "mar_url": "Firefox-mozilla-central-nightly-20211003100640",
                        },
                        "target.partial-3.mar": {
                            "buildid": "20211002213629",
                            "mar_url": "Firefox-mozilla-central-nightly-20211002213629",
                        },
                        "target.partial-4.mar": {
                            "buildid": "20211002095048",
                            "mar_url": "Firefox-mozilla-central-nightly-20211002095048",
                        },
                    }
                }
            },
        )


if __name__ == "__main__":
    main()
