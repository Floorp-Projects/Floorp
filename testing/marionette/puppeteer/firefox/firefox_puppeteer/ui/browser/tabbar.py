# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import (
    By, Wait
)

from marionette_driver.errors import NoSuchElementException

import firefox_puppeteer.errors as errors

from firefox_puppeteer.api.security import Security
from firefox_puppeteer.ui.base import UIBaseLib, DOMElement


class TabBar(UIBaseLib):
    """Wraps the tabs toolbar DOM element inside a browser window."""

    # Properties for visual elements of the tabs toolbar #

    @property
    def menupanel(self):
        """A :class:`MenuPanel` instance which represents the menu panel
        at the far right side of the tabs toolbar.

        :returns: :class:`MenuPanel` instance.
        """
        return MenuPanel(self.marionette, self.window)

    @property
    def newtab_button(self):
        """The DOM element which represents the new tab button.

        :returns: Reference to the new tab button.
        """
        return self.toolbar.find_element(By.ID, 'tabs-newtab-button')

    @property
    def tabs(self):
        """List of all the :class:`Tab` instances of the current browser window.

        :returns: List of :class:`Tab` instances.
        """
        tabs = self.toolbar.find_elements(By.TAG_NAME, 'tab')

        return [Tab(self.marionette, self.window, tab) for tab in tabs]

    @property
    def toolbar(self):
        """The DOM element which represents the tab toolbar.

        :returns: Reference to the tabs toolbar.
        """
        return self.element

    # Properties for helpers when working with the tabs toolbar #

    @property
    def selected_index(self):
        """The index of the currently selected tab.

        :return: Index of the selected tab.
        """
        return int(self.toolbar.get_property('selectedIndex'))

    @property
    def selected_tab(self):
        """A :class:`Tab` instance of the currently selected tab.

        :returns: :class:`Tab` instance.
        """
        return self.tabs[self.selected_index]

    # Methods for helpers when working with the tabs toolbar #

    def close_all_tabs(self, exceptions=None):
        """Forces closing of all open tabs.

        There is an optional `exceptions` list, which can be used to exclude
        specific tabs from being closed.

        :param exceptions: Optional, list of :class:`Tab` instances not to close.
        """
        if exceptions is None:
            exceptions = []

        # Get handles from tab exceptions, and find those which can be closed
        for tab in self.tabs:
            if tab not in exceptions:
                tab.close(force=True)

    def close_tab(self, tab=None, trigger='menu', force=False):
        """Closes the tab by using the specified trigger.

        By default the currently selected tab will be closed. If another :class:`Tab`
        is specified, that one will be closed instead. Also when the tab is closed, a
        :func:`switch_to` call is automatically performed, so that the new selected
        tab becomes active.

        :param tab: Optional, the :class:`Tab` instance to close. Defaults to
         the currently selected tab.

        :param trigger: Optional, method to close the current tab. This can
         be a string with one of `menu` or `shortcut`, or a callback which gets triggered
         with the :class:`Tab` as parameter. Defaults to `menu`.

        :param force: Optional, forces the closing of the window by using the Gecko API.
         Defaults to `False`.
        """
        tab = tab or self.selected_tab
        tab.close(trigger, force)

    def open_tab(self, trigger='menu'):
        """Opens a new tab in the current browser window.

        If the tab opens in the foreground, a call to :func:`switch_to` will
        automatically be performed. But if it opens in the background, the current
        tab will keep its focus.

        It will first verify that a new `<tab>` element has been
        introduced in the tabbar, and then that a new content
        browser has been created.

        :param trigger: Optional, method to open the new tab. This can
         be a string with one of `menu`, `button` or `shortcut`, or a callback
         which gets triggered with the current :class:`Tab` as parameter.
         Defaults to `menu`.

        :returns: :class:`Tab` instance for the opened tab.
        """
        start_handles = self.marionette.window_handles
        start_tabs = self.window.tabbar.tabs

        # Prepare action which triggers the opening of the browser window
        if callable(trigger):
            trigger(self.selected_tab)
        elif trigger == 'button':
            self.window.tabbar.newtab_button.click()
        elif trigger == 'menu':
            self.window.menubar.select_by_id('file-menu', 'menu_newNavigatorTab')
        elif trigger == 'shortcut':
            self.window.send_shortcut(
                self.window.localize_entity('tabCmd.commandkey'),
                accel=True)
        # elif - need to add other cases
        else:
            raise ValueError('Unknown opening method: "%s"' % trigger)

        Wait(self.marionette).until(
            lambda _: len(self.window.tabbar.tabs) == len(start_tabs) + 1,
            message='No new tab present in tabbar')
        Wait(self.marionette).until(
            lambda mn: len(mn.window_handles) == len(start_handles) + 1,
            message='No new content browser created')

        handles = self.marionette.window_handles
        [new_handle] = list(set(handles) - set(start_handles))
        [new_tab] = [tab for tab in self.tabs if tab.handle == new_handle]

        # if the new tab is the currently selected tab, switch to it
        if new_tab == self.selected_tab:
            new_tab.switch_to()

        return new_tab

    def switch_to(self, target):
        """Switches the context to the specified tab.

        :param target: The tab to switch to. `target` can be an index, a :class:`Tab`
         instance, or a callback that returns True in the context of the desired tab.

        :returns: Instance of the selected :class:`Tab`.
        """
        start_handle = self.marionette.current_window_handle

        if isinstance(target, int):
            return self.tabs[target].switch_to()
        elif isinstance(target, Tab):
            return target.switch_to()
        if callable(target):
            for tab in self.tabs:
                tab.switch_to()
                if target(tab):
                    return tab

            self.marionette.switch_to_window(start_handle)
            raise errors.UnknownTabError("No tab found for '{}'".format(target))

        raise ValueError("The 'target' parameter must either be an index or a callable")

    @staticmethod
    def get_handle_for_tab(marionette, tab_element):
        """Retrieves the marionette handle for the given :class:`Tab` instance.

        :param marionette: An instance of the Marionette client.

        :param tab_element: The DOM element corresponding to a tab inside the tabs toolbar.

        :returns: `handle` of the tab.
        """
        # TODO: This introduces coupling with marionette's window handles
        # implementation. To avoid this, the capacity to get the XUL
        # element corresponding to the active window according to
        # marionette or a similar ability should be added to marionette.
        handle = marionette.execute_script("""
          let browser = arguments[0].linkedBrowser;
          if (!browser || browser.outerWindowID == null) {
            return null;
          }
          return browser.outerWindowID.toString();
        """, script_args=[tab_element])

        return handle


class Tab(UIBaseLib):
    """Wraps a tab DOM element."""

    def __init__(self, marionette, window, element):
        super(Tab, self).__init__(marionette, window, element)

        self._security = Security(self.marionette)
        self._handle = None

    # Properties for visual elements of tabs #

    @property
    def close_button(self):
        """The DOM element which represents the tab close button.

        :returns: Reference to the tab close button.
        """
        return self.tab_element.find_element(By.CSS_SELECTOR, '.tab-close-button')

    @property
    def tab_element(self):
        """The inner tab DOM element.

        :returns: Tab DOM element.
        """
        return self.element

    # Properties for backend values

    @property
    def location(self):
        """Returns the current URL

        :returns: Current URL
        """
        self.switch_to()

        return self.marionette.execute_script("""
          return arguments[0].linkedBrowser.currentURI.spec;
        """, script_args=[self.tab_element])

    @property
    def certificate(self):
        """The SSL certificate assiciated with the loaded web page.

        :returns: Certificate details as JSON blob.
        """
        self.switch_to()

        return self._security.get_certificate_for_page(self.tab_element)

    # Properties for helpers when working with tabs #

    @property
    def handle(self):
        """The `handle` of the content window.

        :returns: content window `handle`.
        """
        # If no handle has been set yet, wait until it is available
        if not self._handle:
            self._handle = Wait(self.marionette).until(
                lambda mn: TabBar.get_handle_for_tab(mn, self.element),
                message='Tab handle could not be found.')

        return self._handle

    @property
    def selected(self):
        """Checks if the tab is selected.

        :return: `True` if the tab is selected.
        """
        return self.marionette.execute_script("""
            return arguments[0].hasAttribute('selected');
        """, script_args=[self.tab_element])

    # Methods for helpers when working with tabs #

    def __eq__(self, other):
        return self.handle == other.handle

    def close(self, trigger='menu', force=False):
        """Closes the tab by using the specified trigger.

        To ensure the tab was closed, it will first ensure the
        `<tab>` element is removed from the tabbar, and then confirm
        that the content browser was discarded.

        When the tab is closed a :func:`switch_to` call is automatically performed, so that
        the new selected tab becomes active.

        :param trigger: Optional, method in how to close the tab. This can
         be a string with one of `button`, `menu` or `shortcut`, or a callback which
         gets triggered with the current :class:`Tab` as parameter. Defaults to `menu`.

        :param force: Optional, forces the closing of the window by using the Gecko API.
         Defaults to `False`.
        """
        handle = self.handle
        start_handles = self.marionette.window_handles
        start_tabs = self.window.tabbar.tabs

        self.switch_to()

        if force:
            self.marionette.close()
        elif callable(trigger):
            trigger(self)
        elif trigger == 'button':
            self.close_button.click()
        elif trigger == 'menu':
            self.window.menubar.select_by_id('file-menu', 'menu_close')
        elif trigger == 'shortcut':
            self.window.send_shortcut(self.window.localize_entity('closeCmd.key'),
                                      accel=True)
        else:
            raise ValueError('Unknown closing method: "%s"' % trigger)

        Wait(self.marionette).until(
            lambda _: len(self.window.tabbar.tabs) == len(start_tabs) - 1,
            message='Tab"%s" has not been closed' % handle)
        Wait(self.marionette).until(
            lambda mn: len(mn.window_handles) == len(start_handles) - 1,
            message='Content browser "%s" has not been closed' % handle)

        # Ensure to switch to the window handle which represents the new selected tab
        self.window.tabbar.selected_tab.switch_to()

    def select(self):
        """Selects the tab and sets the focus to it."""
        self.tab_element.click()
        self.switch_to()

        # Bug 1121705: Maybe we have to wait for TabSelect event
        Wait(self.marionette).until(
            lambda _: self.selected,
            message='Tab with handle "%s" could not be selected.' % self.handle)

    def switch_to(self):
        """Switches the context of Marionette to this tab.

        Please keep in mind that calling this method will not select the tab.
        Use the :func:`~Tab.select` method instead.
        """
        self.marionette.switch_to_window(self.handle)


class MenuPanel(UIBaseLib):

    @property
    def popup(self):
        """
        :returns: The :class:`MenuPanelElement`.
        """
        popup = self.marionette.find_element(By.ID, 'PanelUI-popup')
        return self.MenuPanelElement(popup)

    class MenuPanelElement(DOMElement):
        """Wraps the menu panel."""
        _buttons = None

        @property
        def buttons(self):
            """
            :returns: A list of all the clickable buttons in the menu panel.
            """
            if not self._buttons:
                self._buttons = (self.find_element(By.ID, 'PanelUI-multiView')
                                     .find_element(By.ANON_ATTRIBUTE,
                                                   {'anonid': 'viewContainer'})
                                     .find_elements(By.TAG_NAME,
                                                    'toolbarbutton'))
            return self._buttons

        def click(self, target=None):
            """
            Overrides HTMLElement.click to provide a target to click.

            :param target: The label associated with the button to click on,
             e.g., `New Private Window`.
            """
            if not target:
                return DOMElement.click(self)

            for button in self.buttons:
                if button.get_attribute('label') == target:
                    return button.click()
            raise NoSuchElementException('Could not find "{}"" in the '
                                         'menu panel UI'.format(target))
