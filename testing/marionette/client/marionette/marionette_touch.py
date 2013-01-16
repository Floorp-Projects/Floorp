# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

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

    def tap(self, element):
        # we pass in touch/mouse/click events if mouse_event_shim.js isn't included in the gaia app
        # otherwise, we just send touch events. See Bug 829566
        send_all = self.execute_script("return typeof window.wrappedJSObject.MouseEventShim === 'undefined';") 
        self.execute_script("%s.tap(arguments[0], null, null, null, null, arguments[1]);" % self.library_name, [element, send_all])

    def double_tap(self, element):
        self.execute_script("%s.dbltap(arguments[0]);" % self.library_name, [element])

    def long_press(self, element, holdtime=2000):
        #I'm adding holdtime, since we have support for this, and we don't have chaining yet
        self.execute_script("%s.hold.apply(this, arguments);" % self.library_name, [element, holdtime, 0, 0, 0, 0, 0])
        
    def flick(self, element, x1, y1, x2, y2, duration=200):
        # there's 'flick' which is pixels per second, but I'd rather have the library support it than piece it together here.
        self.execute_script("%s.swipe.apply(this, arguments);" % self.library_name, [element, x1, y1, x2, y2, duration])

    def pinch(self, *args, **kwargs):
        raise Exception("Pinch is unsupported")
