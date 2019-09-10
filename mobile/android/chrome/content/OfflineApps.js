/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var OfflineApps = {
  allowSite: function(aDocument) {
    Services.perms.addFromPrincipal(
      aDocument.nodePrincipal,
      "offline-app",
      Services.perms.ALLOW_ACTION
    );

    // When a site is enabled while loading, manifest resources will
    // start fetching immediately.  This one time we need to do it
    // ourselves.
    this._startFetching(aDocument);
  },

  disallowSite: function(aDocument) {
    Services.perms.addFromPrincipal(
      aDocument.nodePrincipal,
      "offline-app",
      Services.perms.DENY_ACTION
    );
  },

  _startFetching: function(aDocument) {
    if (!aDocument.documentElement) {
      return;
    }

    let manifest = aDocument.documentElement.getAttribute("manifest");
    if (!manifest) {
      return;
    }

    let manifestURI = Services.io.newURI(
      manifest,
      aDocument.characterSet,
      aDocument.documentURIObject
    );
    let updateService = Cc[
      "@mozilla.org/offlinecacheupdate-service;1"
    ].getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(
      manifestURI,
      aDocument.documentURIObject,
      aDocument.nodePrincipal,
      window
    );
  },
};
