#!/usr/bin/env python

import io
import os
import re
import shutil
import tempfile
import threading
import time
import unittest
from unittest import mock

import mozunit
import requests
import six
from mozgeckoprofiler import view_gecko_profile

if six.PY2:
    # Import for Python 2
    from urllib import unquote
else:
    # Import for Python 3
    from urllib.parse import unquote


def access_profiler_link(file_url, response):
    """Attempts to access the profile in a loop for 5 seconds.

    This is run from a separate thread.
    """
    timeout = 5  # seconds
    start = time.time()

    while time.time() - start < timeout:
        # Poll the server to try and get a response.
        result = requests.get(url=file_url)
        if result.ok:
            # Return the text back in a list.
            response[0] = result.text
            return
        time.sleep(0.1)

    response[0] = "Accessing the profiler link timed out after %s seconds" % timeout


class TestViewGeckoProfile(unittest.TestCase):
    """Tests the opening local profiles in the Firefox Profiler."""

    def setUp(self):
        self.firefox_profiler_url = None
        self.thread = None
        self.response = [None]

    def test_view_gecko_profile(self):
        # Create a temporary fake performance profile.
        temp_dir = tempfile.mkdtemp()
        profile_path = os.path.join(temp_dir, "fakeprofile.json")
        with io.open(profile_path, "w") as f:
            f.write("FAKE_PROFILE")

        # Mock the open_new_tab function so that we know when the view_gecko_profile
        # function has done all of its work, and we can assert ressult of the
        # user behavior.
        def mocked_open_new_tab(firefox_profiler_url):
            self.firefox_profiler_url = firefox_profiler_url
            encoded_file_url = firefox_profiler_url.split("/")[-1]
            decoded_file_url = unquote(encoded_file_url)
            # Extract the actual file from the path.
            self.thread = threading.Thread(
                target=access_profiler_link, args=(decoded_file_url, self.response)
            )
            print("firefox_profiler_url %s" % firefox_profiler_url)
            print("encoded_file_url %s" % encoded_file_url)
            print("decoded_file_url %s" % decoded_file_url)
            self.thread.start()

        with mock.patch("webbrowser.open_new_tab", new=mocked_open_new_tab):
            # Run the test
            view_gecko_profile(profile_path)

        self.thread.join()

        # Compare the URLs, but replace the PORT value supplied, as that is dynamic.
        expected_url = (
            "https://profiler.firefox.com/from-url/"
            "http%3A%2F%2F127.0.0.1%3A{PORT}%2Ffakeprofile.json"
        )
        actual_url = re.sub("%3A\d+%2F", "%3A{PORT}%2F", self.firefox_profiler_url)

        self.assertEqual(
            actual_url,
            expected_url,
            "The URL generated was correct for the Firefox Profiler.",
        )
        self.assertEqual(
            self.response[0],
            "FAKE_PROFILE",
            "The response from the serve provided the profile contents.",
        )

        shutil.rmtree(temp_dir)


if __name__ == "__main__":
    mozunit.main()
