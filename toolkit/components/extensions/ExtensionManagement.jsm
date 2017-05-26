/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported ExtensionManagement */

this.EXPORTED_SYMBOLS = ["ExtensionManagement"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/*
 * This file should be kept short and simple since it's loaded even
 * when no extensions are running.
 */

let cacheInvalidated = 0;
function onCacheInvalidate() {
  cacheInvalidated++;
}
Services.obs.addObserver(onCacheInvalidate, "startupcache-invalidate");

var ExtensionManagement = {
  get cacheInvalidated() {
    return cacheInvalidated;
  },

  get isExtensionProcess() {
    return WebExtensionPolicy.isExtensionProcess;
  },

  getURLForExtension(id, path = "") {
    let policy = WebExtensionPolicy.getByID(id);
    if (!policy) {
      Cu.reportError(`Called getURLForExtension on unmapped extension ${id}`);
      return null;
    }
    return policy.getURL(path);
  },
};
