# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestPosition(MarionetteTestCase):
    def test_should_get_element_position_back(self):
        doc = """
        <head>
            <title>Rectangles</title>
            <style>
                div {
                    position: absolute;
                    margin: 0;
                    border: 0;
                    padding: 0;
                }
                #r {
                    background-color: red;
                    left: 11px;
                    top: 10px;
                    width: 48.666666667px;
                    height: 49.333333333px;
                }
            </style>
        </head>
        <body>
            <div id="r">r</div>
        </body>
        """
        self.marionette.navigate(inline(doc))

        r2 = self.marionette.find_element(By.ID, "r")
        location = r2.rect
        self.assertEqual(11, location["x"])
        self.assertEqual(10, location["y"])
