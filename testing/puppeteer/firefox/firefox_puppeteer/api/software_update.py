# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re

import mozinfo

from firefox_puppeteer.base import BaseLib
from firefox_puppeteer.api.appinfo import AppInfo
from firefox_puppeteer.api.prefs import Preferences


class ActiveUpdate(BaseLib):

    def __getattr__(self, attr):
        value = self.marionette.execute_script("""
          let ums = Components.classes['@mozilla.org/updates/update-manager;1']
                    .getService(Components.interfaces.nsIUpdateManager);
          return ums.activeUpdate[arguments[0]];
        """, script_args=[attr])

        if value:
            return value
        else:
            raise AttributeError('{} has no attribute {}'.format(self.__class__.__name__,
                                                                 attr))

    @property
    def exists(self):
        """Checks if there is an active update.

        :returns: True if there is an active update
        """
        active_update = self.marionette.execute_script("""
          let ums = Components.classes['@mozilla.org/updates/update-manager;1']
                    .getService(Components.interfaces.nsIUpdateManager);
          return ums.activeUpdate;
        """)

        return bool(active_update)

    def get_patch_at(self, patch_index):
        """Use nsIUpdate.getPatchAt to return a patch from an update.

        :returns: JSON data for an nsIUpdatePatch object
        """
        return self.marionette.execute_script("""
          let ums = Components.classes['@mozilla.org/updates/update-manager;1']
                    .getService(Components.interfaces.nsIUpdateManager);
          return ums.activeUpdate.getPatchAt(arguments[0]);
        """, script_args=[patch_index])

    @property
    def patch_count(self):
        """Get the patchCount from the active update.

        :returns: The patch count
        """
        return self.marionette.execute_script("""
          let ums = Components.classes['@mozilla.org/updates/update-manager;1']
                    .getService(Components.interfaces.nsIUpdateManager);
          return ums.activeUpdate.patchCount;
        """)

    @property
    def selected_patch(self):
        """Get the selected patch for the active update.

        :returns: JSON data for the selected patch
        """
        return self.marionette.execute_script("""
          let ums = Components.classes['@mozilla.org/updates/update-manager;1']
                    .getService(Components.interfaces.nsIUpdateManager);
          return ums.activeUpdate.selectedPatch;
    """)


class MARChannels(BaseLib):
    """Class to handle the allowed MAR channels as listed in update-settings.ini."""
    INI_SECTION = 'Settings'
    INI_OPTION = 'ACCEPTED_MAR_CHANNEL_IDS'

    def __init__(self, marionette_getter):
        BaseLib.__init__(self, marionette_getter)

        self._ini_file_path = self.marionette.execute_script("""
          Components.utils.import('resource://gre/modules/Services.jsm');

          let file = Services.dirsvc.get('GreD', Components.interfaces.nsIFile);
          file.append('update-settings.ini');

          return file.path;
        """)

    @property
    def config_file_path(self):
        """The path to the update-settings.ini file."""
        return self._ini_file_path

    @property
    def config_file_contents(self):
        """The contents of the update-settings.ini file."""
        with open(self.config_file_path) as f:
            return f.read()

    @property
    def channels(self):
        """The channels as found in the ACCEPTED_MAR_CHANNEL_IDS option
        of the update-settings.ini file.

        :returns: A set of channel names
        """
        channels = self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/FileUtils.jsm");
          let iniFactory = Components.classes['@mozilla.org/xpcom/ini-processor-factory;1']
                           .getService(Components.interfaces.nsIINIParserFactory);

          let file = new FileUtils.File(arguments[0]);
          let parser = iniFactory.createINIParser(file);

          return parser.getString(arguments[1], arguments[2]);
        """, script_args=[self.config_file_path, self.INI_SECTION, self.INI_OPTION])
        return set(channels.split(','))

    @channels.setter
    def channels(self, channels):
        """Set the channels in the update-settings.ini file.

        :param channels: A set of channel names
        """
        new_channels = ','.join(channels)
        self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/FileUtils.jsm");
          let iniFactory = Components.classes['@mozilla.org/xpcom/ini-processor-factory;1']
                           .getService(Components.interfaces.nsIINIParserFactory);

          let file = new FileUtils.File(arguments[0]);

          let writer = iniFactory.createINIParser(file)
                       .QueryInterface(Components.interfaces.nsIINIParserWriter);

          writer.setString(arguments[1], arguments[2], arguments[3]);
          writer.writeFile(null, Components.interfaces.nsIINIParserWriter.WRITE_UTF16);
        """, script_args=[self.config_file_path, self.INI_SECTION, self.INI_OPTION, new_channels])

    def add_channels(self, channels):
        """Add channels to the update-settings.ini file.

        :param channels: A set of channel names to add
        """
        self.channels = self.channels | set(channels)

    def remove_channels(self, channels):
        """Remove channels from the update-settings.ini file.

        :param channels: A set of channel names to remove
        """
        self.channels = self.channels - set(channels)


class SoftwareUpdate(BaseLib):
    """The SoftwareUpdate API adds support for an easy access to the update process."""
    PREF_APP_DISTRIBUTION = 'distribution.id'
    PREF_APP_DISTRIBUTION_VERSION = 'distribution.version'
    PREF_APP_UPDATE_URL = 'app.update.url'
    PREF_APP_UPDATE_URL_OVERRIDE = 'app.update.url.override'
    PREF_DISABLED_ADDONS = 'extensions.disabledAddons'

    def __init__(self, marionette_getter):
        BaseLib.__init__(self, marionette_getter)

        self.app_info = AppInfo(marionette_getter)
        self.prefs = Preferences(marionette_getter)

        self._update_channel = UpdateChannel(marionette_getter)
        self._mar_channels = MARChannels(marionette_getter)
        self._active_update = ActiveUpdate(marionette_getter)

    @property
    def ABI(self):
        """Get the customized ABI for the update service.

        :returns: ABI version
        """
        abi = self.app_info.XPCOMABI
        if mozinfo.isMac:
            abi += self.marionette.execute_script("""
              let macutils = Components.classes['@mozilla.org/xpcom/mac-utils;1']
                             .getService(Components.interfaces.nsIMacUtils);
              if (macutils.isUniversalBinary) {
                return '-u-' + macutils.architecturesInBinary;
              }
              return '';
            """)

        return abi

    @property
    def active_update(self):
        """ Holds a reference to an :class:`ActiveUpdate` object."""
        return self._active_update

    @property
    def allowed(self):
        """Check if the user has permissions to run the software update

        :returns: Status if the user has the permissions
        """
        return self.marionette.execute_script("""
          let aus = Components.classes['@mozilla.org/updates/update-service;1']
                    .getService(Components.interfaces.nsIApplicationUpdateService);
          return aus.canCheckForUpdates && aus.canApplyUpdates;
        """)

    @property
    def build_info(self):
        """Return information of the current build version

        :returns: A dictionary of build information
        """
        update_url = self.get_update_url(True)

        return {
            'buildid': self.app_info.appBuildID,
            'channel': self.update_channel.channel,
            'disabled_addons': self.prefs.get_pref(self.PREF_DISABLED_ADDONS),
            'locale': self.app_info.locale,
            'mar_channels': self.mar_channels.channels,
            'update_url': update_url,
            'update_snippet': self.get_update_snippet(update_url),
            'user_agent': self.app_info.user_agent,
            'version': self.app_info.version
        }

    @property
    def is_complete_update(self):
        """Return true if the offered update is a complete update

        :returns: True if the offered update is a complete update
        """
        # Throw when isCompleteUpdate is called without an update. This should
        # never happen except if the test is incorrectly written.
        assert self.active_update.exists, 'An active update has been found'

        patch_count = self.active_update.patch_count
        assert patch_count == 1 or patch_count == 2,\
            'An update must have one or two patches included'

        # Ensure Partial and Complete patches produced have unique urls
        if patch_count == 2:
            patch0_url = self.active_update.get_patch_at(0)['URL']
            patch1_url = self.active_update.get_patch_at(1)['URL']
            assert patch0_url != patch1_url,\
                'Partial and Complete download URLs are different'

        return self.active_update.selected_patch['type'] == 'complete'

    @property
    def mar_channels(self):
        """ Holds a reference to a :class:`MARChannels` object."""
        return self._mar_channels

    @property
    def os_version(self):
        """Returns information about the OS version

        :returns: The OS version
        """
        return self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/Services.jsm");

          let osVersion;
          try {
            osVersion = Services.sysinfo.getProperty("name") + " " +
                        Services.sysinfo.getProperty("version");
          }
          catch (ex) {
          }

          if (osVersion) {
            try {
              osVersion += " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
            }
            catch (e) {
              // Not all platforms have a secondary widget library,
              // so an error is nothing to worry about.
            }
            osVersion = encodeURIComponent(osVersion);
          }
          return osVersion;
        """)

    @property
    def patch_info(self):
        """ Returns information of the active update in the queue."""
        info = {'channel': self.update_channel.channel}

        if (self.active_update.exists):
            info['buildid'] = self.active_update.buildID
            info['is_complete'] = self.is_complete_update
            info['size'] = self.active_update.selected_patch['size']
            info['type'] = self.update_type
            info['url_mirror'] = \
                self.active_update.selected_patch['finalURL'] or 'n/a'
            info['version'] = self.active_update.appVersion

        return info

    @property
    def staging_directory(self):
        """ Returns the path to the updates staging directory."""
        return self.marionette.execute_script("""
          let aus = Components.classes['@mozilla.org/updates/update-service;1']
                    .getService(Components.interfaces.nsIApplicationUpdateService);
          return aus.getUpdatesDirectory().path;
        """)

    @property
    def update_channel(self):
        """ Holds a reference to an :class:`UpdateChannel` object."""
        return self._update_channel

    @property
    def update_type(self):
        """Returns the type of the active update."""
        return self.active_update.type

    def force_fallback(self):
        """Update the update.status file and set the status to 'failed:6'"""
        with open(os.path.join(self.staging_directory, 'update.status'), 'w') as f:
            f.write('failed: 6\n')

    def get_update_snippet(self, update_url):
        """Retrieve contents of the update snippet.

        :param update_url: URL to the update snippet
        """
        snippet = None
        try:
            import urllib2
            response = urllib2.urlopen(update_url)
            snippet = response.read()
        except Exception:
            pass

        return snippet

    def get_update_url(self, force=False):
        """Retrieve the AUS update URL the update snippet is retrieved from.

        :param force: Boolean flag to force an update check

        :returns: The URL of the update snippet
        """
        url = self.prefs.get_pref(self.PREF_APP_UPDATE_URL_OVERRIDE)
        if not url:
            url = self.prefs.get_pref(self.PREF_APP_UPDATE_URL)

        # Format the URL by replacing placeholders
        url = self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/UpdateUtils.jsm")
          return UpdateUtils.formatUpdateURL(arguments[0]);
        """, script_args=[url])

        if force:
            if '?' in url:
                url += '&'
            else:
                url += '?'
            url += 'force=1'

        return url


class UpdateChannel(BaseLib):
    """Class to handle the update channel as listed in channel-prefs.js"""
    REGEX_UPDATE_CHANNEL = re.compile(r'("app\.update\.channel", ")([^"].*)(?=")')

    def __init__(self, marionette_getter):
        BaseLib.__init__(self, marionette_getter)

        self.prefs = Preferences(marionette_getter)

        self.file_path = self.marionette.execute_script("""
          Components.utils.import('resource://gre/modules/Services.jsm');

          let file = Services.dirsvc.get('PrfDef', Components.interfaces.nsIFile);
          file.append('channel-prefs.js');

          return file.path;
        """)

    @property
    def file_contents(self):
        """The contents of the channel-prefs.js file."""
        with open(self.file_path) as f:
            return f.read()

    @property
    def channel(self):
        """The name of the update channel as stored in the
        app.update.channel pref."""
        return self.prefs.get_pref('app.update.channel', True)

    @property
    def default_channel(self):
        """Get the default update channel

        :returns: Current default update channel
        """
        matches = re.search(self.REGEX_UPDATE_CHANNEL, self.file_contents).groups()
        assert len(matches) == 2, 'Update channel value has been found'

        return matches[1]

    @default_channel.setter
    def default_channel(self, channel):
        """Set default update channel.

        :param channel: New default update channel
        """
        assert channel, 'Update channel has been specified'
        new_content = re.sub(
            self.REGEX_UPDATE_CHANNEL, r'\g<1>' + channel, self.file_contents)
        with open(self.file_path, 'w') as f:
            f.write(new_content)
