from marionette import MarionetteTestCase


class TestPosition(MarionetteTestCase):

    def test_should_get_element_position_back(self):
        test_url = self.marionette.absolute_url('rectangles.html')
        self.marionette.navigate(test_url)

        r2 = self.marionette.find_element('id', "r2")
        location = r2.rect
        self.assertEqual(11, location['x'])
        self.assertEqual(10, location['y'])
