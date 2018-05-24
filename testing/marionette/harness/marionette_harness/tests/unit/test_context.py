# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.decorators import using_context
from marionette_driver.errors import MarionetteException
from marionette_harness import MarionetteTestCase


class ContextTestCase(MarionetteTestCase):

    def setUp(self):
        super(ContextTestCase, self).setUp()

        # shortcuts to improve readability of these tests
        self.chrome = self.marionette.CONTEXT_CHROME
        self.content = self.marionette.CONTEXT_CONTENT

        self.assertEquals(self.get_context(), self.content)

        test_url = self.marionette.absolute_url("empty.html")
        self.marionette.navigate(test_url)

    def get_context(self):
        return self.marionette._send_message("getContext", key="value")


class TestSetContext(ContextTestCase):

    def test_switch_context(self):
        self.marionette.set_context(self.chrome)
        self.assertEquals(self.get_context(), self.chrome)

        self.marionette.set_context(self.content)
        self.assertEquals(self.get_context(), self.content)

    def test_invalid_context(self):
        with self.assertRaises(ValueError):
            self.marionette.set_context("foobar")


class TestUsingContext(ContextTestCase):

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

    def test_with_using_context_decorator(self):
        @using_context('content')
        def inner_content(m):
            self.assertEquals(self.get_context(), 'content')

        @using_context('chrome')
        def inner_chrome(m):
            self.assertEquals(self.get_context(), 'chrome')

        inner_content(self.marionette)
        inner_chrome(self.marionette)
