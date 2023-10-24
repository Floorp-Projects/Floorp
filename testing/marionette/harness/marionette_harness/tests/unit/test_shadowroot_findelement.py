from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_driver.errors import (
    DetachedShadowRootException,
    NoSuchElementException,
    NoSuchShadowRootException,
)
from marionette_driver.marionette import WebElement, ShadowRoot

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


page_shadow_dom = inline(
    """
        <custom-element></custom-element>
        <script>
            customElements.define('custom-element',
                class extends HTMLElement {
                    constructor() {
                            super();
                            this.attachShadow({mode: 'open'}).innerHTML = `
                                <div><a href=# id=foo>full link text</a><a href=# id=bar>another link text</a></div>
                            `;
                        }
                });
        </script>"""
)


class TestShadowDOMFindElement(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def test_find_element_from_shadow_root(self):
        self.marionette.navigate(page_shadow_dom)
        custom_element = self.marionette.find_element(By.CSS_SELECTOR, "custom-element")
        shadow_root = custom_element.shadow_root
        found = shadow_root.find_element(By.CSS_SELECTOR, "a")
        self.assertIsInstance(found, WebElement)

        el = self.marionette.execute_script(
            """
            return arguments[0].shadowRoot.querySelector('a')
        """,
            [custom_element],
        )
        self.assertEqual(found, el)

    def test_unknown_element_from_shadow_root(self):
        self.marionette.navigate(page_shadow_dom)
        shadow_root = self.marionette.find_element(
            By.CSS_SELECTOR, "custom-element"
        ).shadow_root
        with self.assertRaises(NoSuchElementException):
            shadow_root.find_element(By.CSS_SELECTOR, "does not exist")

    def test_detached_shadow_root(self):
        self.marionette.navigate(page_shadow_dom)
        shadow_root = self.marionette.find_element(
            By.CSS_SELECTOR, "custom-element"
        ).shadow_root
        self.marionette.refresh()
        with self.assertRaises(DetachedShadowRootException):
            shadow_root.find_element(By.CSS_SELECTOR, "a")

    def test_no_such_shadow_root(self):
        not_existing_shadow_root = ShadowRoot(self.marionette, "foo")
        with self.assertRaises(NoSuchShadowRootException):
            not_existing_shadow_root.find_element(By.CSS_SELECTOR, "a")


class TestShadowDOMFindElements(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def test_find_elements_from_shadow_root(self):
        self.marionette.navigate(page_shadow_dom)
        custom_element = self.marionette.find_element(By.CSS_SELECTOR, "custom-element")
        shadow_root = custom_element.shadow_root
        found = shadow_root.find_elements(By.CSS_SELECTOR, "a")
        self.assertEqual(len(found), 2)

        els = self.marionette.execute_script(
            """
            return arguments[0].shadowRoot.querySelectorAll('a')
        """,
            [custom_element],
        )

        for i in range(len(found)):
            self.assertIsInstance(found[i], WebElement)
            self.assertEqual(found[i], els[i])

    def test_detached_shadow_root(self):
        self.marionette.navigate(page_shadow_dom)
        shadow_root = self.marionette.find_element(
            By.CSS_SELECTOR, "custom-element"
        ).shadow_root
        self.marionette.refresh()
        with self.assertRaises(DetachedShadowRootException):
            shadow_root.find_elements(By.CSS_SELECTOR, "a")

    def test_no_such_shadow_root(self):
        not_existing_shadow_root = ShadowRoot(self.marionette, "foo")
        with self.assertRaises(NoSuchShadowRootException):
            not_existing_shadow_root.find_elements(By.CSS_SELECTOR, "a")
