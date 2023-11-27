#!/usr/bin/env python3

# Copyright (C) 2022 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import json
import os
import sys

from selenium import webdriver
from selenium.common.exceptions import TimeoutException
from selenium.webdriver.common.by import By
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.firefox.service import Service
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait


class SnapTests:
    def __init__(self, exp):
        driver_service = Service(
            executable_path=r"/snap/firefox/current/usr/lib/firefox/geckodriver"
        )
        options = Options()
        options.binary_location = r"/snap/firefox/current/usr/lib/firefox/firefox"
        options.add_argument("--headless")
        self._driver = webdriver.Firefox(service=driver_service, options=options)

        object_methods = [
            method_name
            for method_name in dir(self)
            if callable(getattr(self, method_name))
        ]

        self._wait = WebDriverWait(self._driver, self.get_timeout())

        with open(exp, "r") as j:
            self._expectations = json.load(j)

        try:
            for m in object_methods:
                if m.startswith("test_"):
                    print("Running: {}".format(m))
                    self._driver.save_screenshot("screenshot_{}_pre.png".format(m))
                    getattr(self, m)(self._expectations[m])
                    self._driver.save_screenshot("screenshot_{}_post.png".format(m))
        except TimeoutException:
            self._driver.save_screenshot("screenshot_timeout.png")
        finally:
            self._driver.save_screenshot("screenshot_final.png")
            self._driver.quit()

    def get_timeout(self):
        if "TEST_TIMEOUT" in os.environ.keys():
            return int(os.getenv("TEST_TIMEOUT"))
        else:
            return 5

    def open_tab(self, url):
        opened_tabs = len(self._driver.window_handles)

        self._driver.switch_to.new_window("tab")
        self._wait.until(EC.number_of_windows_to_be(opened_tabs + 1))
        self._driver.get(url)

    def test_about_support(self, exp):
        self.open_tab("about:support")

        version_box = self._wait.until(
            EC.visibility_of_element_located((By.ID, "version-box"))
        )
        self._wait.until(lambda d: len(version_box.text) > 0)
        print("about:support version: {}".format(version_box.text))
        assert version_box.text == exp["version_box"]

        distributionid_box = self._wait.until(
            EC.visibility_of_element_located((By.ID, "distributionid-box"))
        )
        self._wait.until(lambda d: len(distributionid_box.text) > 0)
        print("about:support distribution ID: {}".format(distributionid_box.text))
        assert distributionid_box.text == exp["distribution_id"]

    def test_about_buildconfig(self, exp):
        self.open_tab("about:buildconfig")

        source_link = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "a"))
        )
        self._wait.until(lambda d: len(source_link.text) > 0)
        print("about:buildconfig source: {}".format(source_link.text))
        assert source_link.text.startswith(exp["source_repo"])

        build_flags_box = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "p:last-child"))
        )
        self._wait.until(lambda d: len(build_flags_box.text) > 0)
        print("about:support distribution ID: {}".format(build_flags_box.text))
        assert build_flags_box.text.find(exp["official"]) >= 0

    def test_youtube(self, exp):
        self.open_tab("https://www.youtube.com")

        # Wait for the consent dialog and accept it
        print("Wait for consent form")
        try:
            self._wait.until(
                EC.visibility_of_element_located(
                    (By.CSS_SELECTOR, "button[aria-label*=Accept]")
                )
            ).click()
        except TimeoutException:
            print("Wait for consent form: timed out, maybe it is not here")

        # Find first video and click it
        print("Wait for one video")
        self._wait.until(
            EC.visibility_of_element_located((By.ID, "video-title-link"))
        ).click()

        # Wait for duration to be set to something
        print("Wait for video to start")
        video = self._wait.until(
            EC.visibility_of_element_located((By.CLASS_NAME, "html5-main-video"))
        )
        self._wait.until(lambda d: type(video.get_property("duration")) == float)
        print("video duration: {}".format(video.get_property("duration")))
        assert video.get_property("duration") > exp["duration"]

        self._wait.until(lambda d: video.get_property("currentTime") > exp["playback"])
        print("video played: {}".format(video.get_property("currentTime")))
        assert video.get_property("currentTime") > exp["playback"]


if __name__ == "__main__":
    SnapTests(sys.argv[1])
