# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette_driver.marionette import HTMLElement

from decorators import use_class_as_property


__version__ = '2.0.0'

root = os.path.abspath(os.path.dirname(__file__))
manifest = os.path.join(root, 'tests', 'manifest.ini')


class Puppeteer(object):
    """The puppeteer class is used to expose libraries to test cases.

    Each library can be referenced by its puppeteer name as a member of a
    FirefoxTestCase instance. For example, from within a test method, the
    "current_window" member of the "Browser" class can be accessed via
    "self.browser.current_window".
    """

    def __init__(self):
        self.marionette = None

    def get_marionette(self):
        return self.marionette

    def set_marionette(self, marionette):
        self.marionette = marionette

    @use_class_as_property('api.appinfo.AppInfo')
    def appinfo(self):
        """
        Provides access to members of the appinfo  api.

        See the :class:`~api.appinfo.AppInfo` reference.
        """

    @use_class_as_property('api.keys.Keys')
    def keys(self):
        """
        Provides a definition of control keys to use with keyboard shortcuts.
        For example, keys.CONTROL or keys.ALT.
        See the :class:`~api.keys.Keys` reference.
        """

    @use_class_as_property('api.places.Places')
    def places(self):
        """Provides low-level access to several bookmark and history related actions.

        See the :class:`~api.places.Places` reference.
        """

    @use_class_as_property('api.utils.Utils')
    def utils(self):
        """Provides an api for interacting with utility actions.

        See the :class:`~api.utils.Utils` reference.
        """

    @property
    def platform(self):
        """Returns the lowercased platform name.

        :returns: Platform name
        """
        return self.marionette.session_capabilities['platformName'].lower()

    @use_class_as_property('api.prefs.Preferences')
    def prefs(self):
        """
        Provides an api for setting and inspecting preferences, as see in
        about:config.

        See the :class:`~api.prefs.Preferences` reference.
        """

    @use_class_as_property('api.security.Security')
    def security(self):
        """
        Provides an api for accessing security related properties and helpers.

        See the :class:`~api.security.Security` reference.
        """

    @use_class_as_property('ui.windows.Windows')
    def windows(self):
        """
        Provides shortcuts to the top-level windows.

        See the :class:`~ui.window.Windows` reference.
        """


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

    def get_marionette(self):
        return self.marionette
