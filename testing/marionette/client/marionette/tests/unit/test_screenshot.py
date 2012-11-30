from marionette_test import MarionetteTestCase


class ScreenshotTests(MarionetteTestCase):

    def testWeCanTakeAScreenShotOfAnElement(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        el = self.marionette.find_element('id', 'red')
        self.assertEqual('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAAVUlEQVRoge3PsQ0AIAzAsI78fzBwBhHykD2ePev80LweAAGJB1ILpBZILZBaILVAaoHUAqkFUgukFkgtkFogtUBqgdQCqQVSC6QWSC2QWiC1QGp9A7ma+7nyXgOpzQAAAABJRU5ErkJggg==',
                         self.marionette.screenshot(element=el))

    def testWeCanTakeAScreenShotEntireCanvas(self):
        test_url = self.marionette.absolute_url('html5Page.html')
        self.marionette.navigate(test_url)
        self.assertTrue('data:image/png;base64,iVBORw0KGgo' in
                        self.marionette.screenshot())

