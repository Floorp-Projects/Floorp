# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import ConfigParser
import os
import re
import sys

import mozinfo

from six import reraise

from firefox_puppeteer.base import BaseLib
from firefox_puppeteer.api.appinfo import AppInfo


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

    class MARConfigParser(ConfigParser.ConfigParser):
        """INI parser which allows to read and write MAR config files.

        Virtually identical to the original method, but delimit keys and values
        with '=' instead of ' = '
        """

        def write(self, fp):
            """Write an .ini-format representation of the configuration state."""
            if self._defaults:
                fp.write("[%s]\n" % ConfigParser.DEFAULTSECT)
                for (key, value) in self._defaults.items():
                    fp.write("%s=%s\n" % (key, str(value).replace('\n', '\n\t')))
                fp.write("\n")
            for section in self._sections:
                fp.write("[%s]\n" % section)
                for (key, value) in self._sections[section].items():
                    if key == "__name__":
                        continue
                    if (value is not None) or (self._optcre == self.OPTCRE):
                        key = "=".join((key, str(value).replace('\n', '\n\t')))
                    fp.write("%s\n" % (key))
                fp.write("\n")

    def __init__(self, marionette):
        BaseLib.__init__(self, marionette)

        self.config_file_path = self.marionette.execute_script("""
          Components.utils.import('resource://gre/modules/Services.jsm');

          let file = Services.dirsvc.get('GreD', Components.interfaces.nsIFile);
          file.append('update-settings.ini');

          return file.path;
        """)

        self.config = self.MARConfigParser()
        self.config.optionxform = str

    @property
    def channels(self):
        """The currently accepted MAR channels.

        :returns: A set of channel names
        """
        # Make sure to always read the current file contents
        self.config.read(self.config_file_path)

        return set(self.config.get(self.INI_SECTION, self.INI_OPTION).split(','))

    @channels.setter
    def channels(self, channels):
        """Set the accepted MAR channels.

        :param channels: A set of channel names
        """
        self.config.set(self.INI_SECTION, self.INI_OPTION, ','.join(channels))
        with open(self.config_file_path, 'wb') as configfile:
            self.config.write(configfile)

    def add_channels(self, channels):
        """Add additional MAR channels.

        :param channels: A set of channel names to add
        """
        self.channels = self.channels | set(channels)

    def remove_channels(self, channels):
        """Remove MAR channels.

        :param channels: A set of channel names to remove
        """
        self.channels = self.channels - set(channels)


class SoftwareUpdate(BaseLib):
    """The SoftwareUpdate API adds support for an easy access to the update process."""
    PREF_APP_DISTRIBUTION = 'distribution.id'
    PREF_APP_DISTRIBUTION_VERSION = 'distribution.version'
    PREF_APP_UPDATE_CHANNEL = 'app.update.channel'
    PREF_APP_UPDATE_URL = 'app.update.url'
    PREF_DISABLED_ADDONS = 'extensions.disabledAddons'

    def __init__(self, marionette):
        BaseLib.__init__(self, marionette)

        self.app_info = AppInfo(marionette)

        self._mar_channels = MARChannels(marionette)
        self._active_update = ActiveUpdate(marionette)

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
        update_url = self.get_formatted_update_url(True)

        return {
            'buildid': self.app_info.appBuildID,
            'channel': self.update_channel,
            'disabled_addons': self.marionette.get_pref(self.PREF_DISABLED_ADDONS),
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
        info = {'channel': self.update_channel}

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
        """Return the currently used update channel."""
        return self.marionette.get_pref(self.PREF_APP_UPDATE_CHANNEL,
                                        default_branch=True)

    @update_channel.setter
    def update_channel(self, channel):
        """Set the update channel to be used for update checks.

        :param channel: New update channel to use

        """
        writer = UpdateChannelWriter(self.marionette)
        writer.set_channel(channel)

    @property
    def update_url(self):
        """Return the update URL used for update checks."""
        return self.marionette.get_pref(self.PREF_APP_UPDATE_URL,
                                        default_branch=True)

    @update_url.setter
    def update_url(self, url):
        """Set the update URL to be used for update checks.

        :param url: New update URL to use

        """
        self.marionette.set_pref(self.PREF_APP_UPDATE_URL, url,
                                 default_branch=True)

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
        import urllib2
        try:
            response = urllib2.urlopen(update_url)
            return response.read()
        except urllib2.URLError:
            exc, val, tb = sys.exc_info()
            msg = "Failed to retrieve update snippet '{0}': {1}"
            reraise(Exception, msg.format(update_url, val), tb)

    def get_formatted_update_url(self, force=False):
        """Retrieve the formatted AUS update URL the update snippet is retrieved from.

        :param force: Boolean flag to force an update check

        :returns: The URL of the update snippet
        """
        url = self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/UpdateUtils.jsm");
          let res = UpdateUtils.formatUpdateURL(arguments[0]);
          // Format the URL by replacing placeholders
          // In 56 we switched the method to be async.
          // For now, support both approaches.
          if (res.then) {
            res.then(marionetteScriptFinished);
          } else {
            marionetteScriptFinished(res);
          }
        """, script_args=[self.update_url])

        if force:
            if '?' in url:
                url += '&'
            else:
                url += '?'
            url += 'force=1'

        return url


class UpdateChannelWriter(BaseLib):
    """Class to handle the update channel as listed in channel-prefs.js"""
    REGEX_UPDATE_CHANNEL = re.compile(r'("app\.update\.channel", ")([^"].*)(?=")')

    def __init__(self, *args, **kwargs):
        BaseLib.__init__(self, *args, **kwargs)

        self.file_path = self.marionette.execute_script("""
          Components.utils.import('resource://gre/modules/Services.jsm');

          let file = Services.dirsvc.get('PrfDef', Components.interfaces.nsIFile);
          file.append('channel-prefs.js');

          return file.path;
        """)

    def set_channel(self, channel):
        """Set default update channel.

        :param channel: New default update channel
        """
        with open(self.file_path) as f:
            file_contents = f.read()

        new_content = re.sub(
            self.REGEX_UPDATE_CHANNEL, r'\g<1>' + channel, file_contents)
        with open(self.file_path, 'w') as f:
            f.write(new_content)
