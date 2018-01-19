# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.marionette import HTMLElement

from firefox_puppeteer.base import BaseLib
from firefox_puppeteer.ui.windows import BaseWindow


class UIBaseLib(BaseLib):
    """A base class for all UI element wrapper classes inside a chrome window."""

    def __init__(self, marionette, window, element):
        super(UIBaseLib, self).__init__(marionette)

        assert isinstance(window, BaseWindow)
        assert isinstance(element, HTMLElement)

        self._window = window
        self._element = element

    @property
    def element(self):
        """Returns the reference to the underlying DOM element.

        :returns: Reference to the DOM element
        """
        return self._element

    @property
    def window(self):
        """Returns the reference to the chrome window.

        :returns: :class:`BaseWindow` instance of the chrome window.
        """
        return self._window


class DOMElement(HTMLElement):
    """
    Class that inherits from HTMLElement and provides a way for subclasses to
    expose new api's.
    """

    def __new__(cls, element):
        instance = object.__new__(cls)
        instance.__dict__ = element.__dict__.copy()
        setattr(instance, 'inner', element)

        return instance

    def __init__(self, element):
        pass
