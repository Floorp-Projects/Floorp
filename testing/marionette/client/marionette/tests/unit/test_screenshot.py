import base64
import imghdr

from marionette_test import MarionetteTestCase


RED_ELEMENT_BASE64 = 'iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAAVUlEQVRoge3PsQ0AIAzAsI78fzBwBhHykD2ePev80LweAAGJB1ILpBZILZBaILVAaoHUAqkFUgukFkgtkFogtUBqgdQCqQVSC6QWSC2QWiC1QGp9A7ma+7nyXgOpzQAAAABJRU5ErkJggg=='
GREEN_ELEMENT_BASE64 = 'iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAAV0lEQVRoge3PQRGAQAwAsWINvXgsNnI3+4iAzM7sDWZn9vneoxXRFNEU0RTRFNEU0RTRFNEU0RTRFNEU0RTRFNEU0RTRFNEU0RTRFNEU0RTRFNHcF7nBD/Ha5Ye4BbsYAAAAAElFTkSuQmCC'

class ScreenshotTests(MarionetteTestCase):

    def testWeCanTakeAScreenShotOfEntireViewport(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        content = self.marionette.screenshot()
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        chrome = self.marionette.screenshot()
        # Check the base64 decoded string is a PNG file.
        image = base64.decodestring(chrome)
        self.assertEqual(imghdr.what('', image), 'png')
        self.assertNotEqual(content, chrome)

    def testWeCanTakeAScreenShotOfAnElement(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        el = self.marionette.find_element('id', 'red')
        self.assertEqual(RED_ELEMENT_BASE64,
                         self.marionette.screenshot(element=el))

    def testWeCanTakeAScreenShotWithHighlightOfAnElement(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        el = self.marionette.find_element('id', 'green')
        self.assertEqual(GREEN_ELEMENT_BASE64,
                         self.marionette.screenshot(element=el, highlights=[el]))

    def testWeCanTakeAScreenShotOfEntireCanvas(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        # Check the base64 decoded string is a PNG file.
        image = base64.decodestring(self.marionette.screenshot())
        self.assertEqual(imghdr.what('', image), 'png')

    def testWeCanTakeABinaryScreenShotOfAnElement(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        el = self.marionette.find_element('id', 'red')
        binary_data = self.marionette.screenshot(element=el, format="binary")
        self.assertEqual(RED_ELEMENT_BASE64,
                         base64.b64encode(binary_data))

    def testNotAllowedScreenshotFormatRaiseValueError(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        el = self.marionette.find_element('id', 'red')
        self.assertRaises(ValueError,
                          self.marionette.screenshot,
                          element=el,
                          format="unknowformat")
