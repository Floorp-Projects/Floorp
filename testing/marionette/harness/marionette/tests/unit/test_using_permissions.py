from marionette import MarionetteTestCase
from marionette_driver.errors import JavascriptException


class TestUsingPermssions(MarionetteTestCase):

    def test_using_permissions(self):
        # Test that multiple permissions can be set with 'using_permissions',
        # and that they are set correctly and unset correctly after leaving
        # the context manager.
        original_perm = self.marionette.get_permission('systemXHR')
        original_alarm = self.marionette.get_permission('alarms')
        new_perm = True if original_perm != 1 else False
        new_alarm = True if original_alarm != 1 else False
        with self.marionette.using_permissions({'systemXHR': new_perm,
                                                'alarms': new_alarm}):
            now_perm = self.marionette.get_permission('systemXHR')
            now_alarm = self.marionette.get_permission('alarms')
            self.assertEquals(new_perm, now_perm)
            self.assertNotEquals(now_perm, original_perm)
            self.assertEquals(new_alarm, now_alarm)
            self.assertNotEquals(now_alarm, original_alarm)
        self.assertEquals(original_perm,
                          self.marionette.get_permission('systemXHR'))
        self.assertEquals(original_alarm,
                          self.marionette.get_permission('alarms'))

    def test_exception_using_permissions(self):
        # Test that throwing an exception inside the context manager doesn't
        # prevent the permissions from being restored at context manager exit.
        original_perm = self.marionette.get_permission('systemXHR')
        new_perm = True if original_perm != 1 else False
        with self.marionette.using_permissions({'systemXHR': new_perm}):
            now_perm = self.marionette.get_permission('systemXHR')
            self.assertEquals(new_perm, now_perm)
            self.assertNotEquals(now_perm, original_perm)
            self.assertRaises(JavascriptException,
                              self.marionette.execute_script,
                              "return foo.bar.baz;")
        self.assertEquals(original_perm,
                          self.marionette.get_permission('systemXHR'))
