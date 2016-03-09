from selenium import webdriver
import selenium.webdriver.common.desired_capabilities
import os
import sys
import time
import pytest

def setup_module(module):
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

class TestRemote(object):
    def setup_method(self, method):
        from browsermobproxy.client import Client
        self.client = Client("localhost:9090")

    def teardown_method(self, method):
        self.client.close()

    @pytest.mark.human
    def test_i_want_my_remote(self):
        driver = webdriver.Remote(desired_capabilities=selenium.webdriver.common.desired_capabilities.DesiredCapabilities.FIREFOX,
                                  proxy=self.client)

        self.client.new_har("mtv")
        targetURL = "http://www.mtv.com"
        self.client.rewrite_url(".*american_flag-384x450\\.jpg", "http://www.foodsubs.com/Photos/englishmuffin.jpg")

        driver.get(targetURL)
        time.sleep(5)

        driver.quit()
