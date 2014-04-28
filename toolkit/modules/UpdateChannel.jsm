#filter substitution

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["UpdateChannel"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

this.UpdateChannel = {
  /**
   * Read the update channel from defaults only.  We do this to ensure that
   * the channel is tightly coupled with the application and does not apply
   * to other instances of the application that may use the same profile.
   *
   * @param [optional] aIncludePartners
   *        Whether or not to include the partner bits. Default: true.
   */
  get: function UpdateChannel_get(aIncludePartners = true) {
    let channel = "@MOZ_UPDATE_CHANNEL@";
    let defaults = Services.prefs.getDefaultBranch(null);
    try {
      channel = defaults.getCharPref("app.update.channel");
    } catch (e) {
      // use default value when pref not found
    }

    if (aIncludePartners) {
      try {
        let partners = Services.prefs.getChildList("app.partner.").sort();
        if (partners.length) {
          channel += "-cck";
          partners.forEach(function (prefName) {
            channel += "-" + Services.prefs.getCharPref(prefName);
          });
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }

    return channel;
  }
};
