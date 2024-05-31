# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import errors
from marionette_harness import MarionetteTestCase


class TestSetPermission(MarionetteTestCase):
    def setUp(self):
        super().setUp()
        test_empty = self.marionette.absolute_url("empty.html")
        self.marionette.navigate(test_empty)

    def query_permission(self, descriptor):
        return self.marionette.execute_script(
            """
            return navigator.permissions.query(arguments[0]).then(status => status.state)
            """,
            script_args=(descriptor,),
        )

    def test_granted(self):
        self.marionette.set_permission({"name": "midi"}, "granted")
        self.assertEqual(self.query_permission({"name": "midi"}), "granted")

    def test_denied(self):
        self.marionette.set_permission({"name": "midi"}, "denied")
        self.assertEqual(self.query_permission({"name": "midi"}), "denied")

    def test_prompt(self):
        self.marionette.set_permission({"name": "midi"}, "prompt")
        self.assertEqual(self.query_permission({"name": "midi"}), "prompt")

    def test_invalid_name(self):
        with self.assertRaises(errors.InvalidArgumentException):
            self.marionette.set_permission({"name": "firefox"}, "granted")

    def test_invalid_state(self):
        with self.assertRaises(errors.InvalidArgumentException):
            self.marionette.set_permission({"name": "midi"}, "default")

    def test_extra_flags(self):
        self.marionette.set_permission({"name": "midi"}, "granted")
        self.marionette.set_permission({"name": "midi", "sysex": True}, "prompt")
        self.assertEqual(self.query_permission({"name": "midi"}), "granted")
        self.assertEqual(
            self.query_permission({"name": "midi", "sysex": True}), "prompt"
        )

    def test_storage_access(self):
        self.marionette.set_permission({"name": "storage-access"}, "granted")
        self.assertEqual(self.query_permission({"name": "storage-access"}), "granted")
