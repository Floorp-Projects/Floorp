/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREF_STRIP_ENABLED = "privacy.query_stripping.enabled";

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  // We only init the stripping list service if the pref is on. Otherwise, we
  // add an observer and init the service once the query stripping is enabled.
  if (Services.prefs.getBoolPref(PREF_STRIP_ENABLED)) {
    // Get the query stripping list service to init it.
    Cc["@mozilla.org/query-stripping-list-service;1"].getService(
      Ci.nsIURLQueryStrippingListService
    );
  } else {
    let observer = (subject, topic, data) => {
      if (
        topic == "nsPref:changed" &&
        data == PREF_STRIP_ENABLED &&
        Services.prefs.getBoolPref(PREF_STRIP_ENABLED)
      ) {
        // Init the stripping list service if the query stripping is enabled.
        Cc["@mozilla.org/query-stripping-list-service;1"].getService(
          Ci.nsIURLQueryStrippingListService
        );
        Services.prefs.removeObserver(PREF_STRIP_ENABLED, observer);
      }
    };
    Services.prefs.addObserver(PREF_STRIP_ENABLED, observer);
  }
}
