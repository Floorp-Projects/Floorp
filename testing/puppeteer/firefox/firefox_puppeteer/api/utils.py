# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import MarionetteException

from firefox_puppeteer.base import BaseLib


class Utils(BaseLib):
    """Low-level access to utility actions."""

    def __init__(self, *args, **kwargs):
        BaseLib.__init__(self, *args, **kwargs)

        self._permissions = Permissions(lambda: self.marionette)

    @property
    def permissions(self):
        """Handling of various permissions for hosts.

        :returns: Instance of the Permissions class.
        """
        return self._permissions

    def compare_version(self, a, b):
        """Compare two version strings.

        :param a: The first version.
        :param b: The second version.

        :returns: -1 if a is smaller than b, 0 if equal, and 1 if larger.
        """
        return self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/Services.jsm");
          return Services.vc.compare(arguments[0], arguments[1]);
        """, script_args=[a, b])

    def sanitize(self, data_type):
        """Sanitize user data, including cache, cookies, offlineApps, history, formdata,
        downloads, passwords, sessions, siteSettings.

        Usage:
        sanitize():  Clears all user data.
        sanitize({ "sessions": True }): Clears only session user data.

        more: https://dxr.mozilla.org/mozilla-central/source/browser/base/content/sanitize.js

        :param data_type: optional, Information specifying data to be sanitized
        """

        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_async_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");

              var data_type = arguments[0];

              var data_type = (typeof data_type === "undefined") ? {} : {
                cache: data_type.cache || false,
                cookies: data_type.cookies || false,
                downloads: data_type.downloads || false,
                formdata: data_type.formdata || false,
                history: data_type.history || false,
                offlineApps: data_type.offlineApps || false,
                passwords: data_type.passwords || false,
                sessions: data_type.sessions || false,
                siteSettings: data_type.siteSettings || false
              };

              // Load the sanitize script
              var tempScope = {};
              Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
              .getService(Components.interfaces.mozIJSSubScriptLoader)
              .loadSubScript("chrome://browser/content/sanitize.js", tempScope);

              // Instantiate the Sanitizer
              var s = new tempScope.Sanitizer();
              s.prefDomain = "privacy.cpd.";
              var itemPrefs = Services.prefs.getBranch(s.prefDomain);

              // Apply options for what to sanitize
              for (var pref in data_type) {
                itemPrefs.setBoolPref(pref, data_type[pref]);
              };

              // Sanitize and wait for the promise to resolve
              var finished = false;
              s.sanitize().then(() => {
                for (let pref in data_type) {
                  itemPrefs.clearUserPref(pref);
                };
                marionetteScriptFinished(true);
              }, aError => {
                for (let pref in data_type) {
                  itemPrefs.clearUserPref(pref);
                };
                marionetteScriptFinished(false);
              });
            """, script_args=[data_type])

            if not result:
                raise MarionetteException('Sanitizing of profile data failed.')


class Permissions(BaseLib):

    def add(self, host, permission):
        """Add a permission for web host.

        Permissions include safe-browsing, install, geolocation, and others described here:
        https://dxr.mozilla.org/mozilla-central/source/browser/modules/SitePermissions.jsm#144
        and elsewhere.

        :param host: The web host whose permission will be added.
        :param permission: The type of permission to be added.
        """
        with self.marionette.using_context('chrome'):
            self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");
              let uri = Services.io.newURI(arguments[0], null, null);
              Services.perms.add(uri, arguments[1],
                                 Components.interfaces.nsIPermissionManager.ALLOW_ACTION);
            """, script_args=[host, permission])

    def remove(self, host, permission):
        """Remove a permission for web host.

        Permissions include safe-browsing, install, geolocation, and others described here:
        https://dxr.mozilla.org/mozilla-central/source/browser/modules/SitePermissions.jsm#144
        and elsewhere.

        :param host: The web host whose permission will be removed.
        :param permission: The type of permission to be removed.
        """
        with self.marionette.using_context('chrome'):
            self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");
              let uri = Services.io.newURI(arguments[0], null, null);
              Services.perms.remove(uri, arguments[1]);
            """, script_args=[host, permission])
