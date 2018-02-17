# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import urllib

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


TEST_SIZE = """
    <?xml version="1.0"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
  <head>
    <title>Test page for element size</title>
  </head>
  <body>
    <p>Let's get the size of <a href='#' id='linkId'>some really cool link</a></p>
  </body>
</html>
"""

def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))

class TestElementSize(MarionetteTestCase):
    def testShouldReturnTheSizeOfALink(self):
        self.marionette.navigate(inline(TEST_SIZE))
        shrinko = self.marionette.find_element(By.ID, 'linkId')
        size = shrinko.rect
        self.assertTrue(size['width'] > 0)
        self.assertTrue(size['height'] > 0)
