# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import urllib

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class SelectTestCase(MarionetteTestCase):
    def assertSelected(self, option_element):
        self.assertTrue(option_element.is_selected(), "<option> element not selected")
        self.assertTrue(self.marionette.execute_script(
            "return arguments[0].selected", script_args=[option_element], sandbox=None),
            "<option> selected attribute not updated")

    def assertNotSelected(self, option_element):
        self.assertFalse(option_element.is_selected(), "<option> is selected")
        self.assertFalse(self.marionette.execute_script(
            "return arguments[0].selected", script_args=[option_element], sandbox=None),
            "<option> selected attribute not updated")


class TestSelect(SelectTestCase):
    def test_single(self):
        self.marionette.navigate(inline("""
            <select>
              <option>first
              <option>second
            </select>"""))
        select = self.marionette.find_element(By.TAG_NAME, "select")
        options = self.marionette.find_elements(By.TAG_NAME, "option")

        self.assertSelected(options[0])
        options[1].click()
        self.assertSelected(options[1])

    def test_deselect_others(self):
        self.marionette.navigate(inline("""
          <select>
            <option>first
            <option>second
            <option>third
          </select>"""))
        select = self.marionette.find_element(By.TAG_NAME, "select")
        options = self.marionette.find_elements(By.TAG_NAME, "option")

        options[0].click()
        self.assertSelected(options[0])
        options[1].click()
        self.assertSelected(options[1])
        options[2].click()
        self.assertSelected(options[2])
        options[0].click()
        self.assertSelected(options[0])

    def test_select_self(self):
        self.marionette.navigate(inline("""
          <select>
            <option>first
            <option>second
          </select>"""))
        select = self.marionette.find_element(By.TAG_NAME, "select")
        options = self.marionette.find_elements(By.TAG_NAME, "option")
        self.assertSelected(options[0])
        self.assertNotSelected(options[1])

        options[1].click()
        self.assertSelected(options[1])
        options[1].click()
        self.assertSelected(options[1])

    def test_out_of_view(self):
        self.marionette.navigate(inline("""
          <select>
            <option>1
            <option>2
            <option>3
            <option>4
            <option>5
            <option>6
            <option>7
            <option>8
            <option>9
            <option>10
            <option>11
            <option>12
            <option>13
            <option>14
            <option>15
            <option>16
            <option>17
            <option>18
            <option>19
            <option>20
          </select>"""))
        select = self.marionette.find_element(By.TAG_NAME, "select")
        options = self.marionette.find_elements(By.TAG_NAME, "option")

        options[14].click()
        self.assertSelected(options[14])


class TestSelectMultiple(SelectTestCase):
    def test_single(self):
        self.marionette.navigate(inline("<select multiple> <option>first </select>"))
        option = self.marionette.find_element(By.TAG_NAME, "option")
        option.click()
        self.assertSelected(option)

    def test_multiple(self):
        self.marionette.navigate(inline("""
          <select multiple>
            <option>first
            <option>second
            <option>third
          </select>"""))
        select = self.marionette.find_element(By.TAG_NAME, "select")
        options = select.find_elements(By.TAG_NAME, "option")

        options[1].click()
        self.assertSelected(options[1])

        options[2].click()
        self.assertSelected(options[2])
        self.assertSelected(options[1])

    def test_deselect_selected(self):
        self.marionette.navigate(inline("<select multiple> <option>first </select>"))
        option = self.marionette.find_element(By.TAG_NAME, "option")
        option.click()
        self.assertSelected(option)
        option.click()
        self.assertNotSelected(option)

    def test_deselect_preselected(self):
        self.marionette.navigate(inline("""
          <select multiple>
            <option selected>first
          </select>"""))
        option = self.marionette.find_element(By.TAG_NAME, "option")
        self.assertSelected(option)
        option.click()
        self.assertNotSelected(option)

    def test_out_of_view(self):
        self.marionette.navigate(inline("""
          <select multiple>
            <option>1
            <option>2
            <option>3
            <option>4
            <option>5
            <option>6
            <option>7
            <option>8
            <option>9
            <option>10
            <option>11
            <option>12
            <option>13
            <option>14
            <option>15
            <option>16
            <option>17
            <option>18
            <option>19
            <option>20
          </select>"""))
        select = self.marionette.find_element(By.TAG_NAME, "select")
        options = self.marionette.find_elements(By.TAG_NAME, "option")

        options[-1].click()
        self.assertSelected(options[-1])
