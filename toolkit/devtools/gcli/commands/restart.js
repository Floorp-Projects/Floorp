/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const gcli = require("gcli/index");
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
    name: "restart",
    description: gcli.lookupFormat("restartBrowserDesc", [ BRAND_SHORT_NAME ]),
    params: [
      {
        name: "nocache",
        type: "boolean",
        description: gcli.lookup("restartBrowserNocacheDesc")
      }
    ],
    returnType: "string",
    exec: function Restart(args, context) {
      let canceled = Cc["@mozilla.org/supports-PRBool;1"]
                      .createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(canceled, "quit-application-requested", "restart");
      if (canceled.data) {
        return gcli.lookup("restartBrowserRequestCancelled");
      }

      // disable loading content from cache.
      if (args.nocache) {
        Services.appinfo.invalidateCachesOnRestart();
      }

      // restart
      Cc["@mozilla.org/toolkit/app-startup;1"]
          .getService(Ci.nsIAppStartup)
          .quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      return gcli.lookupFormat("restartBrowserRestarting", [ BRAND_SHORT_NAME ]);
    }
  }
];
