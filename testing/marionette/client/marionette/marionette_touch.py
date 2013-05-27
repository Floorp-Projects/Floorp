# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from errors import ElementNotVisibleException
from marionette import Actions
import gestures
"""
Adds touch support in Marionette
"""
class MarionetteTouchMixin(object):
    """
    Set up the touch layer.
    Can specify another library with a path and the name of the library.
    """
    def setup_touch(self, library=None, library_name=None):
        self.library = library or os.path.abspath(os.path.join(__file__, os.pardir, "touch", "synthetic_gestures.js"))
        self.library_name = library_name or "SyntheticGestures"
        self.import_script(self.library)

    def check_element(self, element):
        if not element.is_displayed():
            raise ElementNotVisibleException

    def tap(self, element):
        self.check_element(element)
        # we pass in touch/mouse/click events if mouse_event_shim.js isn't included in the gaia app
        # otherwise, we just send touch events. See Bug 829566
        send_all = self.execute_script("return typeof window.wrappedJSObject.MouseEventShim === 'undefined';") 
        self.execute_script("%s.tap(arguments[0], null, null, null, null, arguments[1]);" % self.library_name, [element, send_all])

    def double_tap(self, element):
        action = Actions(self)
        action.double_tap(element).perform()

    def flick(self, element, x1, y1, x2, y2, duration=200):
        action = Actions(self)
        action.flick(element, x1, y1, x2, y2, duration).perform()

    def pinch(self, element, x1, y1, x2, y2, x3, y3, x4, y4, duration = 200):
        self.check_element(element)
        gestures.pinch(self, element, x1, y1, x2, y2, x3, y3, x4, y4, duration)
