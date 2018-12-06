from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class ChromeTests(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(ChromeTests, self).setUp()

    def tearDown(self):
        self.close_all_windows()
        super(ChromeTests, self).tearDown()

    def test_hang_until_timeout(self):
        with self.marionette.using_context("chrome"):
            new_window = self.open_window()
        self.marionette.switch_to_window(new_window)

        try:
            try:
                # Raise an exception type which should not be thrown by Marionette
                # while running this test. Otherwise it would mask eg. IOError as
                # thrown for a socket timeout.
                raise NotImplementedError('Exception should not cause a hang when '
                                          'closing the chrome window in content '
                                          'context')
            finally:
                self.marionette.close_chrome_window()
                self.marionette.switch_to_window(self.start_window)
        except NotImplementedError:
            pass
