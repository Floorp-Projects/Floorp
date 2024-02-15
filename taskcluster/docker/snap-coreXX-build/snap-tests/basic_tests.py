#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.


import json
import os
import sys
import traceback

from mozlog import formatters, handlers, structuredlog
from selenium import webdriver
from selenium.common.exceptions import TimeoutException
from selenium.webdriver.common.by import By
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.firefox.service import Service
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait


class SnapTestsBase:
    def __init__(self, exp):
        driver_service = Service(
            executable_path=r"/snap/firefox/current/usr/lib/firefox/geckodriver",
            log_output=os.path.join(
                os.environ.get("ARTIFACT_DIR", ""), "geckodriver.log"
            ),
        )
        options = Options()
        options.binary_location = r"/snap/firefox/current/usr/lib/firefox/firefox"
        if not "TEST_NO_HEADLESS" in os.environ.keys():
            options.add_argument("--headless")
        self._driver = webdriver.Firefox(service=driver_service, options=options)

        self._logger = structuredlog.StructuredLogger(self.__class__.__name__)
        self._logger.add_handler(
            handlers.StreamHandler(sys.stdout, formatters.TbplFormatter())
        )

        test_filter = "test_{}".format(os.environ.get("TEST_FILTER", ""))
        object_methods = [
            method_name
            for method_name in dir(self)
            if callable(getattr(self, method_name))
            and method_name.startswith(test_filter)
        ]

        self._logger.suite_start(object_methods)

        assert self._dir is not None

        self._wait = WebDriverWait(self._driver, self.get_timeout())
        self._longwait = WebDriverWait(self._driver, 20)

        with open(exp, "r") as j:
            self._expectations = json.load(j)

        rv = False
        try:
            first_tab = self._driver.window_handles[0]
            for m in object_methods:
                tabs_before = set(self._driver.window_handles)
                self._driver.switch_to.window(first_tab)
                self._logger.test_start(m)
                self.save_screenshot("screenshot_{}_pre.png".format(m))
                rv = getattr(self, m)(self._expectations[m])
                self._driver.switch_to.parent_frame()
                self.save_screenshot("screenshot_{}_post.png".format(m))
                if rv:
                    self._logger.test_end(m, status="OK")
                else:
                    self._logger.test_end(m, status="FAIL")
                tabs_after = set(self._driver.window_handles)
                tabs_opened = tabs_after - tabs_before
                self._logger.info("opened {} tabs".format(len(tabs_opened)))
                for tab in tabs_opened:
                    self._driver.switch_to.window(tab)
                    self._driver.close()
                self._wait.until(EC.number_of_windows_to_be(len(tabs_before)))
        except TimeoutException:
            rv = False
            self._logger.test_end(m, status="TIMEOUT", message=traceback.format_exc())
            self._driver.switch_to.parent_frame()
            self.save_screenshot("screenshot_timeout.png")
        except AssertionError:
            rv = False
            self._logger.test_end(m, status="FAIL", message=traceback.format_exc())
        except Exception:
            rv = False
            self._logger.test_end(m, status="ERROR", message=traceback.format_exc())
        finally:
            self._driver.switch_to.window(first_tab)
            self.save_screenshot("screenshot_final.png")

        if not "TEST_NO_QUIT" in os.environ.keys():
            self._driver.quit()

        self._logger.info("Exiting with {}".format(rv))
        self._logger.suite_end()
        sys.exit(0 if rv is True else 1)

    def save_screenshot(self, name):
        final_name = name
        if "MOZ_AUTOMATION" in os.environ.keys():
            final_name = os.path.join(os.environ.get("ARTIFACT_DIR"), name)
        self._logger.info("Saving screenshot '{}' to '{}'".format(name, final_name))
        self._driver.save_screenshot(final_name)

    def get_timeout(self):
        if "TEST_TIMEOUT" in os.environ.keys():
            return int(os.getenv("TEST_TIMEOUT"))
        else:
            return 5

    def maybe_collect_reference(self):
        return "TEST_COLLECT_REFERENCE" in os.environ.keys()

    def open_tab(self, url):
        opened_tabs = len(self._driver.window_handles)

        self._driver.switch_to.new_window("tab")
        self._wait.until(EC.number_of_windows_to_be(opened_tabs + 1))
        self._driver.get(url)

        return self._driver.current_window_handle


class SnapTests(SnapTestsBase):
    def __init__(self, exp):
        self._dir = "basic_tests"
        super(SnapTests, self).__init__(exp)

    def test_about_support(self, exp):
        self.open_tab("about:support")

        version_box = self._wait.until(
            EC.visibility_of_element_located((By.ID, "version-box"))
        )
        self._wait.until(lambda d: len(version_box.text) > 0)
        self._logger.info("about:support version: {}".format(version_box.text))
        assert version_box.text == exp["version_box"], "version text should match"

        distributionid_box = self._wait.until(
            EC.visibility_of_element_located((By.ID, "distributionid-box"))
        )
        self._wait.until(lambda d: len(distributionid_box.text) > 0)
        self._logger.info(
            "about:support distribution ID: {}".format(distributionid_box.text)
        )
        assert (
            distributionid_box.text == exp["distribution_id"]
        ), "distribution_id should match"

        windowing_protocol = self._driver.execute_script(
            "return document.querySelector('th[data-l10n-id=\"graphics-window-protocol\"').parentNode.lastChild.textContent;"
        )
        self._logger.info(
            "about:support windowing protocol: {}".format(windowing_protocol)
        )
        assert windowing_protocol == "wayland", "windowing protocol should be wayland"

        return True

    def test_about_buildconfig(self, exp):
        self.open_tab("about:buildconfig")

        source_link = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "a"))
        )
        self._wait.until(lambda d: len(source_link.text) > 0)
        self._logger.info("about:buildconfig source: {}".format(source_link.text))
        assert source_link.text.startswith(
            exp["source_repo"]
        ), "source repo should exists and match"

        build_flags_box = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "p:last-child"))
        )
        self._wait.until(lambda d: len(build_flags_box.text) > 0)
        self._logger.info("about:support buildflags: {}".format(build_flags_box.text))
        assert (
            build_flags_box.text.find(exp["official"]) >= 0
        ), "official build flag should be there"

        return True

    def test_youtube(self, exp):
        self.open_tab("https://www.youtube.com")

        # Wait for the consent dialog and accept it
        self._logger.info("Wait for consent form")
        try:
            self._wait.until(
                EC.visibility_of_element_located(
                    (By.CSS_SELECTOR, "button[aria-label*=Accept]")
                )
            ).click()
        except TimeoutException:
            self._logger.info("Wait for consent form: timed out, maybe it is not here")

        # Find first video and click it
        self._logger.info("Wait for one video")
        self._wait.until(
            EC.visibility_of_element_located((By.ID, "video-title-link"))
        ).click()

        # Wait for duration to be set to something
        self._logger.info("Wait for video to start")
        video = self._wait.until(
            EC.visibility_of_element_located((By.CLASS_NAME, "html5-main-video"))
        )
        self._wait.until(lambda d: type(video.get_property("duration")) == float)
        self._logger.info("video duration: {}".format(video.get_property("duration")))
        assert (
            video.get_property("duration") > exp["duration"]
        ), "youtube video should have duration"

        self._wait.until(lambda d: video.get_property("currentTime") > exp["playback"])
        self._logger.info("video played: {}".format(video.get_property("currentTime")))
        assert (
            video.get_property("currentTime") > exp["playback"]
        ), "youtube video should perform playback"

        return True


if __name__ == "__main__":
    SnapTests(exp=sys.argv[1])
