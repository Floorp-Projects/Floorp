import unittest
import os
import tempfile
import shutil
import responses
import re
import json
import tarfile

from mozprofile.prefs import Preferences

from condprof.client import get_profile, TC_SERVICE, ROOT_URL
from condprof.util import _DEFAULT_SERVER


PROFILE = re.compile(ROOT_URL + "/.*/.*tgz")
PROFILE_FOR_TESTS = os.path.join(os.path.dirname(__file__), "profile")
SECRETS = re.compile(_DEFAULT_SERVER + "/.*")
SECRETS_PROXY = re.compile("http://taskcluster/secrets/.*")


class TestClient(unittest.TestCase):
    def setUp(self):
        self.profile_dir = tempfile.mkdtemp()

        # creating profile.tgz on the fly for serving it
        profile_tgz = os.path.join(self.profile_dir, "profile.tgz")
        with tarfile.open(profile_tgz, "w:gz") as tar:
            tar.add(PROFILE_FOR_TESTS, arcname=".")

        # self.profile_data is the tarball we're sending back via HTTP
        with open(profile_tgz, "rb") as f:
            self.profile_data = f.read()

        self.target = tempfile.mkdtemp()
        self.download_dir = os.path.expanduser("~/.condprof-cache")
        if os.path.exists(self.download_dir):
            shutil.rmtree(self.download_dir)

        responses.add(
            responses.GET,
            PROFILE,
            body=self.profile_data,
            headers={"content-length": str(len(self.profile_data)), "ETag": "'12345'"},
            status=200,
        )

        responses.add(
            responses.HEAD,
            PROFILE,
            body="",
            headers={"content-length": str(len(self.profile_data)), "ETag": "'12345'"},
            status=200,
        )

        responses.add(responses.HEAD, TC_SERVICE, body="", status=200)

        secret = {"secret": {"username": "user", "password": "pass"}}
        secret = json.dumps(secret)
        for pattern in (SECRETS, SECRETS_PROXY):
            responses.add(
                responses.GET,
                pattern,
                body=secret,
                headers={"content-length": str(len(secret))},
                status=200,
            )

    def tearDown(self):
        shutil.rmtree(self.target)
        shutil.rmtree(self.download_dir)
        shutil.rmtree(self.profile_dir)

    @responses.activate
    def test_cache(self):
        download_dir = os.path.expanduser("~/.condprof-cache")
        if os.path.exists(download_dir):
            num_elmts = len(os.listdir(download_dir))
        else:
            num_elmts = 0

        get_profile(self.target, "win64", "settled", "default")

        # grabbing a profile should generate two files
        self.assertEqual(len(os.listdir(download_dir)), num_elmts + 2)

        # we do at least two network calls when getting a file,
        # a HEAD and a GET and possibly a TC secret
        self.assertTrue(len(responses.calls) >= 2)

        # reseting the response counters
        responses.calls.reset()

        # and we should reuse them without downloading the file again
        get_profile(self.target, "win64", "settled", "default")

        # grabbing a profile should not download new stuff
        self.assertEqual(len(os.listdir(download_dir)), num_elmts + 2)

        # and do a single extra HEAD call, everything else is cached,
        # even the TC secret
        self.assertEqual(len(responses.calls), 2)

        prefs_js = os.path.join(self.target, "prefs.js")
        prefs = Preferences.read_prefs(prefs_js)

        # check that the gfx.blacklist prefs where cleaned out
        for name, value in prefs:
            self.assertFalse(name.startswith("gfx.blacklist"))

        # check that we have the startupScanScopes option forced
        prefs = dict(prefs)
        self.assertEqual(prefs["extensions.startupScanScopes"], 1)

        # make sure we don't have any marionette option set
        user_js = os.path.join(self.target, "user.js")
        for name, value in Preferences.read_prefs(user_js):
            self.assertFalse(name.startswith("marionette."))


if __name__ == "__main__":
    try:
        import mozunit
    except ImportError:
        pass
    else:
        mozunit.main(runwith="unittest")
