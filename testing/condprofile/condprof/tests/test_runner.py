import os
import unittest
import tempfile
import shutil
import re

import responses
from condprof.runner import main

GECKODRIVER = os.path.join(os.path.dirname(__file__), "fakegeckodriver.py")
FIREFOX = os.path.join(os.path.dirname(__file__), "fakefirefox.py")
CHANGELOG = re.compile("https://index.taskcluster.net/.*/changelog.json")
FTP = "https://ftp.mozilla.org/pub/firefox/nightly/latest-mozilla-central/"
PROFILE = re.compile("https://index.taskcluster.net/.*/.*tgz")

with open(os.path.join(os.path.dirname(__file__), "profile.tgz"), "rb") as f:
    PROFILE_DATA = f.read()

with open(os.path.join(os.path.dirname(__file__), "ftp_mozilla.html")) as f:
    FTP_PAGE = f.read()

FTP_ARCHIVE = re.compile(
    "https://ftp.mozilla.org/pub/firefox/nightly/" "latest-mozilla-central/firefox.*"
)

ADDON = re.compile("https://addons.mozilla.org/.*/.*xpi")


class TestRunner(unittest.TestCase):
    def setUp(self):
        self.archive_dir = tempfile.mkdtemp()
        responses.add(responses.GET, CHANGELOG, json={"error": "not found"}, status=404)
        responses.add(
            responses.GET,
            FTP,
            content_type="application/text/html",
            body=FTP_PAGE,
            status=200,
        )

        responses.add(
            responses.GET,
            FTP_ARCHIVE,
            body="1",
            headers={"content-length": "1"},
            status=200,
        )

        responses.add(
            responses.GET,
            PROFILE,
            body=PROFILE_DATA,
            headers={"content-length": str(len(PROFILE_DATA))},
            status=200,
        )

        responses.add(
            responses.HEAD,
            PROFILE,
            body="",
            headers={"content-length": str(len(PROFILE_DATA))},
            status=200,
        )

        responses.add(responses.HEAD, FTP_ARCHIVE, body="", status=200)

        responses.add(
            responses.GET, ADDON, body="1", headers={"content-length": "1"}, status=200
        )

        responses.add(
            responses.HEAD, ADDON, body="", headers={"content-length": "1"}, status=200
        )

    def tearDown(self):
        shutil.rmtree(self.archive_dir)

    @responses.activate
    def test_runner(self):
        args = ["--geckodriver", GECKODRIVER, "--firefox", FIREFOX, self.archive_dir]
        main(args)
        # XXX we want a bunch of assertions here to check
        # that the archives dir gets filled correctly
