# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By

from firefox_puppeteer.ui.base import UIBaseLib
from firefox_puppeteer.ui.deck import Panel


class Deck(UIBaseLib):

    def _create_panel_for_id(self, panel_id):
        """Creates an instance of :class:`Panel` for the specified panel id.

        :param panel_id: The ID of the panel to create an instance of.

        :returns: :class:`Panel` instance
        """
        mapping = {'apply': ApplyPanel,
                   'checkForUpdates': CheckForUpdatesPanel,
                   'checkingForUpdates': CheckingForUpdatesPanel,
                   'downloadAndInstall': DownloadAndInstallPanel,
                   'downloadFailed': DownloadFailedPanel,
                   'downloading': DownloadingPanel,
                   'noUpdatesFound': NoUpdatesFoundPanel,
                   }

        panel = self.element.find_element(By.ID, panel_id)
        return mapping.get(panel_id, Panel)(self.marionette, self.window, panel)

    # Properties for visual elements of the deck #

    @property
    def apply(self):
        """The :class:`ApplyPanel` instance for the apply panel.

        :returns: :class:`ApplyPanel` instance.
        """
        return self._create_panel_for_id('apply')

    @property
    def check_for_updates(self):
        """The :class:`CheckForUpdatesPanel` instance for the check for updates panel.

        :returns: :class:`CheckForUpdatesPanel` instance.
        """
        return self._create_panel_for_id('checkForUpdates')

    @property
    def checking_for_updates(self):
        """The :class:`CheckingForUpdatesPanel` instance for the checking for updates panel.

        :returns: :class:`CheckingForUpdatesPanel` instance.
        """
        return self._create_panel_for_id('checkingForUpdates')

    @property
    def download_and_install(self):
        """The :class:`DownloadAndInstallPanel` instance for the download and install panel.

        :returns: :class:`DownloadAndInstallPanel` instance.
        """
        return self._create_panel_for_id('downloadAndInstall')

    @property
    def download_failed(self):
        """The :class:`DownloadFailedPanel` instance for the download failed panel.

        :returns: :class:`DownloadFailedPanel` instance.
        """
        return self._create_panel_for_id('downloadFailed')

    @property
    def downloading(self):
        """The :class:`DownloadingPanel` instance for the downloading panel.

        :returns: :class:`DownloadingPanel` instance.
        """
        return self._create_panel_for_id('downloading')

    @property
    def no_updates_found(self):
        """The :class:`NoUpdatesFoundPanel` instance for the no updates found panel.

        :returns: :class:`NoUpdatesFoundPanel` instance.
        """
        return self._create_panel_for_id('noUpdatesFound')

    @property
    def panels(self):
        """List of all the :class:`Panel` instances of the current deck.

        :returns: List of :class:`Panel` instances.
        """
        panels = self.marionette.execute_script("""
          let deck = arguments[0];
          let panels = [];

          for (let index = 0; index < deck.children.length; index++) {
            if (deck.children[index].id) {
              panels.push(deck.children[index].id);
            }
          }

          return panels;
        """, script_args=[self.element])

        return [self._create_panel_for_id(panel) for panel in panels]

    @property
    def selected_index(self):
        """The index of the currently selected panel.

        :return: Index of the selected panel.
        """
        return int(self.element.get_property('selectedIndex'))

    @property
    def selected_panel(self):
        """A :class:`Panel` instance of the currently selected panel.

        :returns: :class:`Panel` instance.
        """
        return self.panels[self.selected_index]


class ApplyPanel(Panel):

    @property
    def button(self):
        """The DOM element which represents the Update button.

        :returns: Reference to the button element.
        """
        return self.element.find_element(By.ID, 'updateButton')


class CheckForUpdatesPanel(Panel):

    @property
    def button(self):
        """The DOM element which represents the Check for Updates button.

        :returns: Reference to the button element.
        """
        return self.element.find_element(By.ID, 'checkForUpdatesButton')


class CheckingForUpdatesPanel(Panel):
    pass


class DownloadAndInstallPanel(Panel):

    @property
    def button(self):
        """The DOM element which represents the Download button.

        :returns: Reference to the button element.
        """
        return self.element.find_element(By.ID, 'downloadAndInstallButton')


class DownloadFailedPanel(Panel):
    pass


class DownloadingPanel(Panel):
    pass


class NoUpdatesFoundPanel(Panel):
    pass
