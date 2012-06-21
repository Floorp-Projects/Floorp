/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SocialService"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");

const MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");

const SocialService = {

  _init: function _init() {
    let origins = MANIFEST_PREFS.getChildList("", {});
    this._providers = origins.reduce(function (memo, origin) {
      try {
        var manifest = JSON.parse(MANIFEST_PREFS.getCharPref(origin));
      }
      catch (err) {}
      if (manifest && typeof(manifest) == "object") {
        memo[manifest.origin] = Object.create(manifest);
      }
      return memo;
    }, {}, this);
  },

  getProvider: function getProvider(origin, onDone) {
    schedule((function () {
      onDone(this._providers[origin] || null);
    }).bind(this));
  },
};

SocialService._init();

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}
