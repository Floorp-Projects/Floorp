from __future__ import absolute_import

from selenium import webdriver
import selenium.webdriver.common.desired_capabilities
from selenium.webdriver.common.proxy import Proxy
import os
import sys
import copy
import time
import pytest

def setup_module(module):
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

class TestWebDriver(object):
    def setup_method(self, method):
        from browsermobproxy.client import Client
        self.client = Client("localhost:9090")

    def teardown_method(self, method):
        self.client.close()

    @pytest.mark.human
    def test_i_want_my_by_capability(self):
        capabilities = selenium.webdriver.common.desired_capabilities.DesiredCapabilities.FIREFOX
        self.client.add_to_capabilities(capabilities)
        driver = webdriver.Firefox(capabilities=capabilities)

        self.client.new_har("mtv")
        targetURL = "http://www.mtv.com"
        self.client.rewrite_url(".*american_flag-384x450\\.jpg", "http://www.foodsubs.com/Photos/englishmuffin.jpg")

        driver.get(targetURL)

        time.sleep(5)

        driver.quit()

    @pytest.mark.human
    def test_i_want_my_by_proxy_object(self):
        driver = webdriver.Firefox(proxy=self.client)

        self.client.new_har("mtv")
        targetURL = "http://www.mtv.com"
        self.client.rewrite_url(".*american_flag-384x450\\.jpg", "http://www.foodsubs.com/Photos/englishmuffin.jpg")

        driver.get(targetURL)

        time.sleep(5)

        driver.quit()

    def test_what_things_look_like(self):
        bmp_capabilities = copy.deepcopy(selenium.webdriver.common.desired_capabilities.DesiredCapabilities.FIREFOX)
        self.client.add_to_capabilities(bmp_capabilities)

        proxy_capabilities = copy.deepcopy(selenium.webdriver.common.desired_capabilities.DesiredCapabilities.FIREFOX)
        proxy_addr = 'localhost:{}'.format(self.client.port)
        proxy = Proxy({'httpProxy': proxy_addr,'sslProxy': proxy_addr})
        proxy.add_to_capabilities(proxy_capabilities)

        assert bmp_capabilities == proxy_capabilities
