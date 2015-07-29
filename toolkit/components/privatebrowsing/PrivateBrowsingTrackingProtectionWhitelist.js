/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function PrivateBrowsingTrackingProtectionWhitelist() {
  // The list of URIs explicitly excluded from tracking protection.
  this._allowlist = [];

  Services.obs.addObserver(this, "last-pb-context-exited", true);
}

PrivateBrowsingTrackingProtectionWhitelist.prototype = {
  classID: Components.ID("{a319b616-c45d-4037-8d86-01c592b5a9af}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrivateBrowsingTrackingProtectionWhitelist,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PrivateBrowsingTrackingProtectionWhitelist),

  /**
   * Add the provided URI to the list of allowed tracking sites.
   *
   * @param uri nsIURI
   *        The URI to add to the list.
   */
  addToAllowList(uri) {
    if (this._allowlist.indexOf(uri.spec) === -1) {
      this._allowlist.push(uri.spec);
    }
  },

  /**
   * Remove the provided URI from the list of allowed tracking sites.
   *
   * @param uri nsIURI
   *        The URI to add to the list.
   */
  removeFromAllowList(uri) {
    let index = this._allowlist.indexOf(uri.spec);
    if (index !== -1) {
      this._allowlist.splice(index, 1);
    }
  },

  /**
   * Check if the provided URI exists in the list of allowed tracking sites.
   *
   * @param uri nsIURI
   *        The URI to add to the list.
   */
  existsInAllowList(uri) {
    return this._allowlist.indexOf(uri.spec) !== -1;
  },

  observe: function (subject, topic, data) {
    if (topic == "last-pb-context-exited") {
      this._allowlist = [];
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PrivateBrowsingTrackingProtectionWhitelist]);
