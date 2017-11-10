import urllib

from marionette_driver.by import By
from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class RenderedElementTests(MarionetteTestCase):

    def test_get_computed_style_value_from_element(self):
        self.marionette.navigate(inline("""
            <div style="color: green;" id="parent">
              <p id="green">This should be green</p>
              <p id="red" style="color: red;">But this is red</p>
            </div>
            """))

        parent = self.marionette.find_element(By.ID, "parent")
        self.assertEqual("rgb(0, 128, 0)", parent.value_of_css_property("color"))

        green = self.marionette.find_element(By.ID, "green")
        self.assertEqual("rgb(0, 128, 0)", green.value_of_css_property("color"))

        red = self.marionette.find_element(By.ID, "red")
        self.assertEqual("rgb(255, 0, 0)", red.value_of_css_property("color"))
