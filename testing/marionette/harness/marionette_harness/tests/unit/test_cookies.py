# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import calendar
import random
import time

from marionette_driver.errors import UnsupportedOperationException
from marionette_harness import MarionetteTestCase


class CookieTest(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        test_url = self.marionette.absolute_url('test.html')
        self.marionette.navigate(test_url)
        self.COOKIE_A = {"name": "foo",
                         "value": "bar",
                         "path": "/",
                         "secure": False}

    def tearDown(self):
        self.marionette.delete_all_cookies()
        MarionetteTestCase.tearDown(self)

    def test_add_cookie(self):
        self.marionette.add_cookie(self.COOKIE_A)
        cookie_returned = str(self.marionette.execute_script("return document.cookie"))
        self.assertTrue(self.COOKIE_A["name"] in cookie_returned)

    def test_adding_a_cookie_that_expired_in_the_past(self):
        cookie = self.COOKIE_A.copy()
        cookie["expiry"] = calendar.timegm(time.gmtime()) - (60 * 60 * 24)
        self.marionette.add_cookie(cookie)
        cookies = self.marionette.get_cookies()
        self.assertEquals(0, len(cookies))

    def test_chrome_error(self):
        with self.marionette.using_context("chrome"):
            self.assertRaises(UnsupportedOperationException,
                              self.marionette.add_cookie, self.COOKIE_A)
            self.assertRaises(UnsupportedOperationException,
                              self.marionette.delete_cookie, self.COOKIE_A)
            self.assertRaises(UnsupportedOperationException,
                              self.marionette.delete_all_cookies)
            self.assertRaises(UnsupportedOperationException,
                              self.marionette.get_cookies)

    def test_delete_all_cookie(self):
        self.marionette.add_cookie(self.COOKIE_A)
        cookie_returned = str(self.marionette.execute_script("return document.cookie"))
        print(cookie_returned)
        self.assertTrue(self.COOKIE_A["name"] in cookie_returned)
        self.marionette.delete_all_cookies()
        self.assertFalse(self.marionette.get_cookies())

    def test_delete_cookie(self):
        self.marionette.add_cookie(self.COOKIE_A)
        cookie_returned = str(self.marionette.execute_script("return document.cookie"))
        self.assertTrue(self.COOKIE_A["name"] in cookie_returned)
        self.marionette.delete_cookie("foo")
        cookie_returned = str(self.marionette.execute_script("return document.cookie"))
        self.assertFalse(self.COOKIE_A["name"] in cookie_returned)

    def test_should_get_cookie_by_name(self):
        key = "key_{}".format(int(random.random()*10000000))
        self.marionette.execute_script("document.cookie = arguments[0] + '=set';", [key])

        cookie = self.marionette.get_cookie(key)
        self.assertEquals("set", cookie["value"])

    def test_get_all_cookies(self):
        key1 = "key_{}".format(int(random.random()*10000000))
        key2 = "key_{}".format(int(random.random()*10000000))

        cookies = self.marionette.get_cookies()
        count = len(cookies)

        one = {"name" :key1,
               "value": "value"}
        two = {"name":key2,
               "value": "value"}

        self.marionette.add_cookie(one)
        self.marionette.add_cookie(two)

        test_url = self.marionette.absolute_url('test.html')
        self.marionette.navigate(test_url)
        cookies = self.marionette.get_cookies()
        self.assertEquals(count + 2, len(cookies))

    def test_should_not_delete_cookies_with_a_similar_name(self):
        cookieOneName = "fish"
        cookie1 = {"name" :cookieOneName,
                    "value":"cod"}
        cookie2 = {"name" :cookieOneName + "x",
                    "value": "earth"}
        self.marionette.add_cookie(cookie1)
        self.marionette.add_cookie(cookie2)

        self.marionette.delete_cookie(cookieOneName)
        cookies = self.marionette.get_cookies()

        self.assertFalse(cookie1["name"] == cookies[0]["name"], msg=str(cookies))
        self.assertEquals(cookie2["name"] , cookies[0]["name"], msg=str(cookies))

    def test_we_get_required_elements_when_available(self):
        self.marionette.add_cookie(self.COOKIE_A)
        cookies = self.marionette.get_cookies()

        self.assertIn("name", cookies[0], 'name not available')
        self.assertIn("value", cookies[0], 'value not available')
        self.assertIn("httpOnly", cookies[0], 'httpOnly not available')
