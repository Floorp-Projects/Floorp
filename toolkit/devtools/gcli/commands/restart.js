/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const l10n = require("gcli/l10n");
const Services = require("Services");

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"]
                           .getService(Ci.nsIStringBundleService)
                           .createBundle("chrome://branding/locale/brand.properties")
                           .GetStringFromName("brandShortName");

/**
 * Restart command
 *
 * @param boolean nocache
 *        Disables loading content from cache upon restart.
 *
 * Examples :
 * >> restart
 * - restarts browser immediately
 * >> restart --nocache
 * - restarts immediately and starts Firefox without using cache
 */
exports.items = [
  {
    item: "command",
    runAt: "client",
    name: "restart",
    description: l10n.lookupFormat("restartBrowserDesc", [ BRAND_SHORT_NAME ]),
    params: [{
      group: l10n.lookup("restartBrowserGroupOptions"),
      params: [
        {
          name: "nocache",
          type: "boolean",
          description: l10n.lookup("restartBrowserNocacheDesc")
        },
        {
          name: "safemode",
          type: "boolean",
          description: l10n.lookup("restartBrowserSafemodeDesc")
        }
      ]
    }],
    returnType: "string",
    exec: function Restart(args, context) {
      let canceled = Cc["@mozilla.org/supports-PRBool;1"]
                      .createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(canceled, "quit-application-requested", "restart");
      if (canceled.data) {
        return l10n.lookup("restartBrowserRequestCancelled");
      }

      // disable loading content from cache.
      if (args.nocache) {
        Services.appinfo.invalidateCachesOnRestart();
      }

      const appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                           .getService(Ci.nsIAppStartup);

      if (args.safemode) {
        // restart in safemode
        appStartup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
      } else {
        // restart normally
        appStartup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      }

      return l10n.lookupFormat("restartBrowserRestarting", [ BRAND_SHORT_NAME ]);
    }
  }
];
