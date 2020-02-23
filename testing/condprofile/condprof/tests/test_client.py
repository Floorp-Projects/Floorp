import unittest
import os
import tempfile
import shutil
import responses
import re
import json

from condprof.client import get_profile, ROOT_URL
from condprof.util import _DEFAULT_SERVER


PROFILE = re.compile(ROOT_URL + "/.*/.*tgz")
with open(os.path.join(os.path.dirname(__file__), "profile.tgz"), "rb") as f:
    PROFILE_DATA = f.read()
SECRETS = re.compile(_DEFAULT_SERVER + "/.*")
SECRETS_PROXY = re.compile("http://taskcluster/secrets/.*")


class TestClient(unittest.TestCase):
    def setUp(self):
        self.target = tempfile.mkdtemp()
        self.download_dir = os.path.expanduser("~/.condprof-cache")
        if os.path.exists(self.download_dir):
            shutil.rmtree(self.download_dir)

        responses.add(
            responses.GET,
            PROFILE,
            body=PROFILE_DATA,
            headers={"content-length": str(len(PROFILE_DATA)), "ETag": "'12345'"},
            status=200,
        )

        responses.add(
            responses.HEAD,
            PROFILE,
            body="",
            headers={"content-length": str(len(PROFILE_DATA)), "ETag": "'12345'"},
            status=200,
        )

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
        self.assertEqual(len(responses.calls), 1)


if __name__ == "__main__":
    try:
        import mozunit
    except ImportError:
        pass
    else:
        mozunit.main(runwith="unittest")
