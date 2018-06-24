# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By, Wait
from marionette_driver.errors import NoSuchWindowException
from marionette_driver.keys import Keys

import firefox_puppeteer.errors as errors

from firefox_puppeteer.api.l10n import L10n
from firefox_puppeteer.base import BaseLib
from firefox_puppeteer.decorators import use_class_as_property


class Windows(BaseLib):

    # Used for registering the different windows with this class to avoid
    # circular dependencies with BaseWindow
    windows_map = {}

    @property
    def all(self):
        """Retrieves a list of all open chrome windows.

        :returns: List of :class:`BaseWindow` instances corresponding to the
                  windows in `marionette.chrome_window_handles`.
        """
        return [self.create_window_instance(handle) for handle in
                self.marionette.chrome_window_handles]

    @property
    def current(self):
        """Retrieves the currently selected chrome window.

        :returns: The :class:`BaseWindow` for the currently active window.
        """
        return self.create_window_instance(self.marionette.current_chrome_window_handle)

    @property
    def focused_chrome_window_handle(self):
        """Returns the currently focused chrome window handle.

        :returns: The `window handle` of the focused chrome window.
        """
        def get_active_handle(mn):
            with self.marionette.using_context('chrome'):
                return self.marionette.execute_script("""
                  Components.utils.import("resource://gre/modules/Services.jsm");

                  let win = Services.focus.activeWindow;
                  if (win) {
                    return win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIDOMWindowUtils)
                              .outerWindowID.toString();
                  }

                  return null;
                """)

        # In case of `None` being returned no window is currently active. This can happen
        # when a focus change action is currently happening. So lets wait until it is done.
        return Wait(self.marionette).until(get_active_handle,
                                           message='No focused window has been found.')

    def close(self, handle):
        """Closes the chrome window with the given handle.

        :param handle: The handle of the chrome window.
        """
        self.switch_to(handle)

        # TODO: Maybe needs to wait as handled via an observer
        return self.marionette.close_chrome_window()

    def close_all(self, exceptions=None):
        """Closes all open chrome windows.

        There is an optional `exceptions` list, which can be used to exclude
        specific chrome windows from being closed.

        :param exceptions: Optional, list of :class:`BaseWindow` instances not to close.
        """
        windows_to_keep = exceptions or []

        # Get handles of windows_to_keep
        handles_to_keep = [entry.handle for entry in windows_to_keep]

        # Find handles to close and close them all
        handles_to_close = set(self.marionette.chrome_window_handles) - set(handles_to_keep)
        for handle in handles_to_close:
            self.close(handle)

    def create_window_instance(self, handle, expected_class=None):
        """Creates a :class:`BaseWindow` instance for the given chrome window.

        :param handle: The handle of the chrome window.
        :param expected_class: Optional, check for the correct window class.
        """
        current_handle = self.marionette.current_chrome_window_handle
        window = None

        with self.marionette.using_context('chrome'):
            try:
                # Retrieve window type to determine the type of chrome window
                if handle != self.marionette.current_chrome_window_handle:
                    self.switch_to(handle)
                window_type = self.marionette.get_window_type()
            finally:
                # Ensure to switch back to the original window
                if handle != current_handle:
                    self.switch_to(current_handle)

            if window_type in self.windows_map:
                window = self.windows_map[window_type](self.marionette, handle)
            else:
                window = BaseWindow(self.marionette, handle)

            if expected_class is not None and type(window) is not expected_class:
                raise errors.UnexpectedWindowTypeError('Expected window "%s" but got "%s"' %
                                                       (expected_class, type(window)))

            # Before continuing ensure the chrome window has been completed loading
            Wait(self.marionette).until(
                lambda _: self.loaded(handle),
                message='Chrome window with handle "%s" did not finish loading.' % handle)

        return window

    def focus(self, handle):
        """Focuses the chrome window with the given handle.

        :param handle: The handle of the chrome window.
        """
        self.switch_to(handle)

        with self.marionette.using_context('chrome'):
            self.marionette.execute_script(""" window.focus(); """)

        Wait(self.marionette).until(
            lambda _: handle == self.focused_chrome_window_handle,
            message='Focus has not been set to chrome window handle "%s".' % handle)

    def loaded(self, handle):
        """Check if the chrome window with the given handle has been completed loading.

        :param handle: The handle of the chrome window.

        :returns: True, if the chrome window has been loaded.
        """
        with self.marionette.using_context('chrome'):
            return self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");

              let win = Services.wm.getOuterWindowWithId(Number(arguments[0]));
              return win.document.readyState == 'complete';
            """, script_args=[handle])

    def switch_to(self, target):
        """Switches context to the specified chrome window.

        :param target: The window to switch to. `target` can be a `handle` or a
                       callback that returns True in the context of the desired
                       window.

        :returns: Instance of the selected :class:`BaseWindow`.
        """
        target_handle = None

        if target in self.marionette.chrome_window_handles:
            target_handle = target
        elif callable(target):
            current_handle = self.marionette.current_chrome_window_handle

            # switches context if callback for a chrome window returns `True`.
            for handle in self.marionette.chrome_window_handles:
                self.marionette.switch_to_window(handle)
                window = self.create_window_instance(handle)
                if target(window):
                    target_handle = handle
                    break

            # if no handle has been found switch back to original window
            if not target_handle:
                self.marionette.switch_to_window(current_handle)

        if target_handle is None:
            raise NoSuchWindowException("No window found for '{}'"
                                        .format(target))

        self.marionette.switch_to_window(target_handle)

        return self.create_window_instance(target_handle)

    @classmethod
    def register_window(cls, window_type, window_class):
        """Registers a chrome window with this class so that this class may in
        turn create the appropriate window instance later on.

        :param window_type: The type of window.
        :param window_class: The constructor of the window
        """
        cls.windows_map[window_type] = window_class


class BaseWindow(BaseLib):
    """Base class for any kind of chrome window."""

    # l10n class attributes will be set by each window class individually
    dtds = []
    properties = []

    def __init__(self, marionette, window_handle):
        super(BaseWindow, self).__init__(marionette)

        self._l10n = L10n(self.marionette)
        self._windows = Windows(self.marionette)

        if window_handle not in self.marionette.chrome_window_handles:
            raise errors.UnknownWindowError('Window with handle "%s" does not exist' %
                                            window_handle)
        self._handle = window_handle

    def __eq__(self, other):
        return self.handle == other.handle

    @property
    def closed(self):
        """Returns closed state of the chrome window.

        :returns: True if the window has been closed.
        """
        return self.handle not in self.marionette.chrome_window_handles

    @property
    def focused(self):
        """Returns `True` if the chrome window is focused.

        :returns: True if the window is focused.
        """
        self.switch_to()

        return self.handle == self._windows.focused_chrome_window_handle

    @property
    def handle(self):
        """Returns the `window handle` of the chrome window.

        :returns: `window handle`.
        """
        return self._handle

    @property
    def loaded(self):
        """Checks if the window has been fully loaded.

        :returns: True, if the window is loaded.
        """
        self._windows.loaded(self.handle)

    @use_class_as_property('ui.menu.MenuBar')
    def menubar(self):
        """Provides access to the menu bar, for example, the **File** menu.

        See the :class:`~ui.menu.MenuBar` reference.
        """

    @property
    def window_element(self):
        """Returns the inner DOM window element.

        :returns: DOM window element.
        """
        self.switch_to()

        return self.marionette.find_element(By.CSS_SELECTOR, ':root')

    def close(self, callback=None, force=False):
        """Closes the current chrome window.

        If this is the last remaining window, the Marionette session is ended.

        :param callback: Optional, function to trigger the window to open. It is
         triggered with the current :class:`BaseWindow` as parameter.
         Defaults to `window.open()`.

        :param force: Optional, forces the closing of the window by using the Gecko API.
         Defaults to `False`.
        """
        self.switch_to()

        # Bug 1121698
        # For more stable tests register an observer topic first
        prev_win_count = len(self.marionette.chrome_window_handles)

        handle = self.handle
        if force or callback is None:
            self._windows.close(handle)
        else:
            callback(self)

        # Bug 1121698
        # Observer code should let us ditch this wait code
        Wait(self.marionette).until(
            lambda m: len(m.chrome_window_handles) == prev_win_count - 1,
            message='Chrome window with handle "%s" has not been closed.' % handle)

    def focus(self):
        """Sets the focus to the current chrome window."""
        return self._windows.focus(self.handle)

    def localize_entity(self, entity_id):
        """Returns the localized string for the specified DTD entity id.

        :param entity_id: The id to retrieve the value from.

        :returns: The localized string for the requested entity.

        :raises MarionetteException: When entity id is not found.
        """
        return self._l10n.localize_entity(self.dtds, entity_id)

    def localize_property(self, property_id):
        """Returns the localized string for the specified property id.

        :param property_id: The id to retrieve the value from.

        :returns: The localized string for the requested property.

        :raises MarionetteException: When property id is not found.
        """
        return self._l10n.localize_property(self.properties, property_id)

    def open_window(self, callback=None, expected_window_class=None, focus=True):
        """Opens a new top-level chrome window.

        :param callback: Optional, function to trigger the window to open. It is
         triggered with the current :class:`BaseWindow` as parameter.
         Defaults to `window.open()`.
        :param expected_class: Optional, check for the correct window class.
        :param focus: Optional, if true, focus the new window.
         Defaults to `True`.
        """
        # Bug 1121698
        # For more stable tests register an observer topic first
        start_handles = self.marionette.chrome_window_handles

        self.switch_to()
        with self.marionette.using_context('chrome'):
            if callback is not None:
                callback(self)
            else:
                self.marionette.execute_script(""" window.open(); """)

        # TODO: Needs to be replaced with observer handling code (bug 1121698)
        def window_opened(mn):
            return len(mn.chrome_window_handles) == len(start_handles) + 1
        Wait(self.marionette).until(
            window_opened,
            message='No new chrome window has been opened.')

        handles = self.marionette.chrome_window_handles
        [new_handle] = list(set(handles) - set(start_handles))

        assert new_handle is not None

        window = self._windows.create_window_instance(new_handle, expected_window_class)

        if focus:
            window.focus()

        return window

    def send_shortcut(self, command_key, **kwargs):
        """Sends a keyboard shortcut to the window.

        :param command_key: The key (usually a letter) to be pressed.

        :param accel: Optional, If `True`, the `Accel` modifier key is pressed.
         This key differs between OS X (`Meta`) and Linux/Windows (`Ctrl`). Defaults to `False`.

        :param alt: Optional, If `True`, the `Alt` modifier key is pressed. Defaults to `False`.

        :param ctrl: Optional, If `True`, the `Ctrl` modifier key is pressed. Defaults to `False`.

        :param meta: Optional, If `True`, the `Meta` modifier key is pressed. Defaults to `False`.

        :param shift: Optional, If `True`, the `Shift` modifier key is pressed.
         Defaults to `False`.
        """

        platform = self.marionette.session_capabilities['platformName']

        keymap = {
            'accel': Keys.META if platform == "mac" else Keys.CONTROL,
            'alt': Keys.ALT,
            'cmd': Keys.COMMAND,
            'ctrl': Keys.CONTROL,
            'meta': Keys.META,
            'shift': Keys.SHIFT,
        }

        # Append all to press modifier keys
        keys = []
        for modifier in kwargs:
            if modifier not in keymap:
                raise KeyError('"%s" is not a known modifier' % modifier)

            if kwargs[modifier] is True:
                keys.append(keymap[modifier])

        # Bug 1125209 - Only lower-case command keys should be sent
        keys.append(command_key.lower())

        self.switch_to()

        with self.marionette.using_context("chrome"):
            self.window_element.send_keys(*keys)

    def switch_to(self, focus=False):
        """Switches the context to this chrome window.

        By default it will not focus the window. If that behavior is wanted, the
        `focus` parameter can be used.

        :param focus: If `True`, the chrome window will be focused.

        :returns: Current window as :class:`BaseWindow` instance.
        """
        if focus:
            self._windows.focus(self.handle)
        else:
            self._windows.switch_to(self.handle)

        return self
