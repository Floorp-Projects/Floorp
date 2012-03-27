# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1 
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/ # 
# Software distributed under the License is distributed on an "AS IS" basis, 
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
# for the specific language governing rights and limitations under the 
# License. 
# 
# The Original Code is Marionette Client. 
# 
# The Initial Developer of the Original Code is 
#   Mozilla Foundation. 
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved. 
# 
# Contributor(s): 
# 
# Alternatively, the contents of this file may be used under the terms of 
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"), 
# in which case the provisions of the GPL or the LGPL are applicable instead 
# of those above. If you wish to allow use of your version of this file only 
# under the terms of either the GPL or the LGPL, and not to allow others to 
# use your version of this file under the terms of the MPL, indicate your 
# decision by deleting the provisions above and replace them with the notice 
# and other provisions required by the GPL or the LGPL. If you do not delete 
# the provisions above, a recipient may use your version of this file under 
# the terms of any one of the MPL, the GPL or the LGPL. 
# 
# ***** END LICENSE BLOCK ***** 

import os
from marionette_test import MarionetteTestCase
from marionette import HTMLElement
from errors import NoSuchElementException

class TestElements(MarionetteTestCase):
    def test_id(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("id", "mozLink")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_tag_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByTagName('body')[0];")
        found_el = self.marionette.find_element("tag name", "body")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_class_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByClassName('linkClass')[0];")
        found_el = self.marionette.find_element("class name", "linkClass")
        self.assertEqual(HTMLElement, type(found_el));
        self.assertTrue(el.id, found_el.id)

    def test_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByName('myInput')[0];")
        found_el = self.marionette.find_element("name", "myInput")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)
    
    def test_selector(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('testh1');")
        found_el = self.marionette.find_element("css selector", "h1")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_link_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("link text", "Click me!")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_partial_link_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("partial link text", "Click m")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_xpath(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("xpath", "id('mozLink')")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_not_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "I'm not on the page")

    def test_timeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "newDiv")
        self.assertTrue(True, self.marionette.set_search_timeout(4000))
        self.marionette.navigate(test_html)
        self.assertEqual(HTMLElement, type(self.marionette.find_element("id", "newDiv")))

class TestElementsChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")

    def test_id(self):
        el = self.marionette.execute_script("return window.document.getElementById('main-window');")
        found_el = self.marionette.find_element("id", "main-window")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_tag_name(self):
        el = self.marionette.execute_script("return window.document.getElementsByTagName('window')[0];")
        found_el = self.marionette.find_element("tag name", "window")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertTrue(el.id, found_el.id)

    def test_class_name(self):
        el = self.marionette.execute_script("return window.document.getElementsByClassName('editBookmarkPanelHeaderButton')[0];")
        found_el = self.marionette.find_element("class name", "editBookmarkPanelHeaderButton")
        self.assertEqual(HTMLElement, type(found_el));
        self.assertTrue(el.id, found_el.id)

    def test_xpath(self):
        el = self.marionette.execute_script("return window.document.getElementById('main-window');")
        found_el = self.marionette.find_element("xpath", "id('main-window')")
        self.assertEqual(HTMLElement, type(found_el));
        self.assertTrue(el.id, found_el.id)

    def test_not_found(self):
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "I'm not on the page")


    def test_timeout(self):
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "myid")
        self.assertTrue(True, self.marionette.set_search_timeout(4000))
        self.marionette.execute_script("window.setTimeout(function() {var b = window.document.createElement('button'); b.id = 'myid'; document.getElementById('main-window').appendChild(b);}, 1000)")
        self.assertEqual(HTMLElement, type(self.marionette.find_element("id", "myid")))
        self.marionette.execute_script("window.document.getElementById('main-window').removeChild(window.document.getElementById('myid'));")
