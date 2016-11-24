# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver import By


class TestHttpdServer(MarionetteTestCase):

    def test_handler(self):
        status = {"count": 0}

        def handler(request, response):
            status["count"] += 1

            response.headers.set("Content-Type", "text/html")
            response.content = "<html><body><p id=\"count\">{}</p></body></html>".format(
                status["count"])

            return ()

        route = ("GET", "/httpd/test_handler", handler)
        self.httpd.router.register(*route)

        url = self.marionette.absolute_url("httpd/test_handler")

        for counter in range(0, 5):
            self.marionette.navigate(url)
            self.assertEqual(status["count"], counter + 1)
            elem = self.marionette.find_element(By.ID, "count")
            self.assertEqual(elem.text, str(counter + 1))
