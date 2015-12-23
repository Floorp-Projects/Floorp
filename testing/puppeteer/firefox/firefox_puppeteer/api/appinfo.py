# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer.base import BaseLib


class AppInfo(BaseLib):
    """This class provides access to various attributes of AppInfo.

    For more details on AppInfo, visit:
    https://developer.mozilla.org/en-US/docs/Mozilla/QA/Mozmill_tests/Shared_Modules/UtilsAPI/appInfo
    """

    def __getattr__(self, attr):
        with self.marionette.using_context('chrome'):
            value = self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");

              return Services.appinfo[arguments[0]];
            """, script_args=[attr])

            if value is not None:
                return value
            else:
                raise AttributeError('{} has no attribute {}'.format(self.__class__.__name__,
                                                                     attr))

    @property
    def locale(self):
        with self.marionette.using_context('chrome'):
            return self.marionette.execute_script("""
              return Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                               .getService(Components.interfaces.nsIXULChromeRegistry)
                               .getSelectedLocale("global");
            """)

    @property
    def user_agent(self):
        with self.marionette.using_context('chrome'):
            return self.marionette.execute_script("""
              return Components.classes["@mozilla.org/network/protocol;1?name=http"]
                               .getService(Components.interfaces.nsIHttpProtocolHandler)
                               .userAgent;
            """)
