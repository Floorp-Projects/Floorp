import ConfigParser
import json
import os
import sys
import unittest

from webserver import Httpd
from network import get_lan_ip

repo_root = os.path.abspath(os.path.join(__file__, "../.."))
sys.path.insert(1, os.path.join(repo_root, "tools", "webdriver"))
from webdriver.driver import WebDriver
from webdriver import exceptions, wait


class WebDriverBaseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.driver = create_driver()

        cls.webserver = Httpd(host=get_lan_ip())
        cls.webserver.start()

    @classmethod
    def tearDownClass(cls):
        cls.webserver.stop()
        if cls.driver:
            cls.driver.quit()


def create_driver():
    config = ConfigParser.ConfigParser()
    config.read('webdriver.cfg')
    section = os.environ.get("WD_BROWSER", 'firefox')
    url = 'http://127.0.0.1:4444/wd/hub'
    if config.has_option(section, 'url'):
        url = config.get(section, "url")
    capabilities = None
    if config.has_option(section, 'capabilities'):
        try:
            capabilities = json.loads(config.get(section, "capabilities"))
        except:
            pass
    mode = 'compatibility'
    if config.has_option(section, 'mode'):
        mode = config.get(section, 'mode')

    return WebDriver(url, {}, capabilities, mode)
