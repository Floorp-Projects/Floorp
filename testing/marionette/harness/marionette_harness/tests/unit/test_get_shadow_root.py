# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_driver.errors import (
    DetachedShadowRootException,
    NoSuchShadowRootException,
)
from marionette_driver.marionette import ShadowRoot
from marionette_harness import MarionetteTestCase

checkbox_dom = """
        <style>
            custom-checkbox-element {
                display:block; width:20px; height:20px;
            }
        </style>
        <custom-checkbox-element></custom-checkbox-element>
        <script>
            customElements.define('custom-checkbox-element',
                class extends HTMLElement {
                    constructor() {
                            super();
                            this.attachShadow({mode: '%s'}).innerHTML = `
                                <div><input type="checkbox"/></div>
                            `;
                        }
                });
        </script>"""


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestShadowDom(MarionetteTestCase):
    def setUp(self):
        super(TestShadowDom, self).setUp()

    def test_can_get_open_shadow_root(self):
        self.marionette.navigate(inline(checkbox_dom % "open"))
        element = self.marionette.find_element(
            By.CSS_SELECTOR, "custom-checkbox-element"
        )
        shadow_root = element.shadow_root
        assert isinstance(
            shadow_root, ShadowRoot
        ), "Should have received ShadowRoot but got {}".format(shadow_root)

    def test_can_get_closed_shadow_root(self):
        self.marionette.navigate(inline(checkbox_dom % "closed"))
        element = self.marionette.find_element(
            By.CSS_SELECTOR, "custom-checkbox-element"
        )
        shadow_root = element.shadow_root
        assert isinstance(
            shadow_root, ShadowRoot
        ), "Should have received ShadowRoot but got {}".format(shadow_root)

    def test_cannot_find_shadow_root(self):
        element = self.marionette.find_element(By.CSS_SELECTOR, "style")
        with self.assertRaises(NoSuchShadowRootException):
            element.shadow_root
