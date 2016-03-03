# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait

from firefox_puppeteer.ui_base_lib import UIBaseLib
from firefox_puppeteer.ui.deck import Panel


class Wizard(UIBaseLib):

    def __init__(self, *args, **kwargs):
        UIBaseLib.__init__(self, *args, **kwargs)

        Wait(self.marionette).until(lambda _: self.selected_panel)

    def _create_panel_for_id(self, panel_id):
        """Creates an instance of :class:`Panel` for the specified panel id.

        :param panel_id: The ID of the panel to create an instance of.

        :returns: :class:`Panel` instance
        """
        mapping = {'checking': CheckingPanel,
                   'downloading': DownloadingPanel,
                   'dummy': DummyPanel,
                   'errorpatching': ErrorPatchingPanel,
                   'errors': ErrorPanel,
                   'errorextra': ErrorExtraPanel,
                   'finished': FinishedPanel,
                   'finishedBackground': FinishedBackgroundPanel,
                   'incompatibleCheck': IncompatibleCheckPanel,
                   'incompatibleList': IncompatibleListPanel,
                   'installed': InstalledPanel,
                   'license': LicensePanel,
                   'manualUpdate': ManualUpdatePanel,
                   'noupdatesfound': NoUpdatesFoundPanel,
                   'pluginupdatesfound': PluginUpdatesFoundPanel,
                   'updatesfoundbasic': UpdatesFoundBasicPanel,
                   'updatesfoundbillboard': UpdatesFoundBillboardPanel,
                   }

        panel = self.element.find_element(By.ID, panel_id)
        return mapping.get(panel_id, Panel)(lambda: self.marionette, self.window, panel)

    # Properties for visual buttons of the wizard #

    @property
    def _buttons(self):
        return self.element.find_element(By.ANON_ATTRIBUTE, {'anonid': 'Buttons'})

    @property
    def cancel_button(self):
        return self._buttons.find_element(By.ANON_ATTRIBUTE, {'dlgtype': 'cancel'})

    @property
    def extra1_button(self):
        return self._buttons.find_element(By.ANON_ATTRIBUTE, {'dlgtype': 'extra1'})

    @property
    def extra2_button(self):
        return self._buttons.find_element(By.ANON_ATTRIBUTE, {'dlgtype': 'extra2'})

    @property
    def previous_button(self):
        return self._buttons.find_element(By.ANON_ATTRIBUTE, {'dlgtype': 'back'})

    @property
    def finish_button(self):
        return self._buttons.find_element(By.ANON_ATTRIBUTE, {'dlgtype': 'finish'})

    @property
    def next_button(self):
        return self._buttons.find_element(By.ANON_ATTRIBUTE, {'dlgtype': 'next'})

    # Properties for visual panels of the wizard #

    @property
    def checking(self):
        """The checking for updates panel.

        :returns: :class:`CheckingPanel` instance.
        """
        return self._create_panel_for_id('checking')

    @property
    def downloading(self):
        """The downloading panel.

        :returns: :class:`DownloadingPanel` instance.
        """
        return self._create_panel_for_id('downloading')

    @property
    def dummy(self):
        """The dummy panel.

        :returns: :class:`DummyPanel` instance.
        """
        return self._create_panel_for_id('dummy')

    @property
    def error_patching(self):
        """The error patching panel.

        :returns: :class:`ErrorPatchingPanel` instance.
        """
        return self._create_panel_for_id('errorpatching')

    @property
    def error(self):
        """The errors panel.

        :returns: :class:`ErrorPanel` instance.
        """
        return self._create_panel_for_id('errors')

    @property
    def error_extra(self):
        """The error extra panel.

        :returns: :class:`ErrorExtraPanel` instance.
        """
        return self._create_panel_for_id('errorextra')

    @property
    def finished(self):
        """The finished panel.

        :returns: :class:`FinishedPanel` instance.
        """
        return self._create_panel_for_id('finished')

    @property
    def finished_background(self):
        """The finished background panel.

        :returns: :class:`FinishedBackgroundPanel` instance.
        """
        return self._create_panel_for_id('finishedBackground')

    @property
    def incompatible_check(self):
        """The incompatible check panel.

        :returns: :class:`IncompatibleCheckPanel` instance.
        """
        return self._create_panel_for_id('incompatibleCheck')

    @property
    def incompatible_list(self):
        """The incompatible list panel.

        :returns: :class:`IncompatibleListPanel` instance.
        """
        return self._create_panel_for_id('incompatibleList')

    @property
    def installed(self):
        """The installed panel.

        :returns: :class:`InstalledPanel` instance.
        """
        return self._create_panel_for_id('installed')

    @property
    def license(self):
        """The license panel.

        :returns: :class:`LicensePanel` instance.
        """
        return self._create_panel_for_id('license')

    @property
    def manual_update(self):
        """The manual update panel.

        :returns: :class:`ManualUpdatePanel` instance.
        """
        return self._create_panel_for_id('manualUpdate')

    @property
    def no_updates_found(self):
        """The no updates found panel.

        :returns: :class:`NoUpdatesFoundPanel` instance.
        """
        return self._create_panel_for_id('noupdatesfound')

    @property
    def plugin_updates_found(self):
        """The plugin updates found panel.

        :returns: :class:`PluginUpdatesFoundPanel` instance.
        """
        return self._create_panel_for_id('pluginupdatesfound')

    @property
    def updates_found_basic(self):
        """The updates found panel.

        :returns: :class:`UpdatesFoundPanel` instance.
        """
        return self._create_panel_for_id('updatesfoundbasic')

    @property
    def updates_found_billboard(self):
        """The billboard panel shown if an update has been found.

        :returns: :class:`UpdatesFoundBillboardPanel` instance.
        """
        return self._create_panel_for_id('updatesfoundbillboard')

    @property
    def panels(self):
        """List of all the available :class:`Panel` instances.

        :returns: List of :class:`Panel` instances.
        """
        panels = self.marionette.execute_script("""
          let wizard = arguments[0];
          let panels = [];

          for (let index = 0; index < wizard.children.length; index++) {
            if (wizard.children[index].id) {
              panels.push(wizard.children[index].id);
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
        return int(self.element.get_attribute('pageIndex'))

    @property
    def selected_panel(self):
        """A :class:`Panel` instance of the currently selected panel.

        :returns: :class:`Panel` instance.
        """
        return self._create_panel_for_id(self.element.get_attribute('currentpageid'))


class CheckingPanel(Panel):

    @property
    def progress(self):
        """The DOM element which represents the progress meter.

        :returns: Reference to the progress element.
        """
        return self.element.find_element(By.ID, 'checkingProgress')


class DownloadingPanel(Panel):

    @property
    def progress(self):
        """The DOM element which represents the progress meter.

        :returns: Reference to the progress element.
        """
        return self.element.find_element(By.ID, 'downloadProgress')


class DummyPanel(Panel):
    pass


class ErrorPatchingPanel(Panel):
    pass


class ErrorPanel(Panel):
    pass


class ErrorExtraPanel(Panel):
    pass


class FinishedPanel(Panel):
    pass


class FinishedBackgroundPanel(Panel):
    pass


class IncompatibleCheckPanel(Panel):

    @property
    def progress(self):
        """The DOM element which represents the progress meter.

        :returns: Reference to the progress element.
        """
        return self.element.find_element(By.ID, 'incompatibleCheckProgress')


class IncompatibleListPanel(Panel):
    pass


class InstalledPanel(Panel):
    pass


class LicensePanel(Panel):
    pass


class ManualUpdatePanel(Panel):
    pass


class NoUpdatesFoundPanel(Panel):
    pass


class PluginUpdatesFoundPanel(Panel):
    pass


class UpdatesFoundBasicPanel(Panel):
    pass


class UpdatesFoundBillboardPanel(Panel):
    pass
