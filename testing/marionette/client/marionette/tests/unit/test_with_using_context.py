# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import MarionetteException


class TestSetContext(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)

        # shortcuts to improve readability of these tests
        self.chrome = self.marionette.CONTEXT_CHROME
        self.content = self.marionette.CONTEXT_CONTENT

        test_url = self.marionette.absolute_url('empty.html')
        self.marionette.navigate(test_url)
        self.marionette.set_context(self.content)
        self.assertEquals(self.get_context(), self.content)

    def get_context(self):
        return self.marionette._send_message('getContext', 'value')

    def test_set_different_context_using_with_block(self):
        with self.marionette.using_context(self.chrome):
            self.assertEquals(self.get_context(), self.chrome)
        self.assertEquals(self.get_context(), self.content)

    def test_set_same_context_using_with_block(self):
        with self.marionette.using_context(self.content):
            self.assertEquals(self.get_context(), self.content)
        self.assertEquals(self.get_context(), self.content)

    def test_nested_with_blocks(self):
        with self.marionette.using_context(self.chrome):
            self.assertEquals(self.get_context(), self.chrome)
            with self.marionette.using_context(self.content):
                self.assertEquals(self.get_context(), self.content)
            self.assertEquals(self.get_context(), self.chrome)
        self.assertEquals(self.get_context(), self.content)

    def test_set_scope_while_in_with_block(self):
        with self.marionette.using_context(self.chrome):
            self.assertEquals(self.get_context(), self.chrome)
            self.marionette.set_context(self.content)
            self.assertEquals(self.get_context(), self.content)
        self.assertEquals(self.get_context(), self.content)

    def test_exception_raised_while_in_with_block_is_propagated(self):
        with self.assertRaises(MarionetteException):
            with self.marionette.using_context(self.chrome):
                raise MarionetteException
        self.assertEquals(self.get_context(), self.content)
