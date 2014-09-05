/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

function ActivitiesGlue() { }

ActivitiesGlue.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIActivityUIGlue]),
  classID: Components.ID("{e4deb5f6-d5e3-4fce-bc53-901dd9951c48}"),

  // Ignore aActivities results on Android, go straight to Android intents.
  chooseActivity: function ap_chooseActivity(aOptions, aActivities, aCallback) {
    Messaging.sendRequestForResult({
      type: "WebActivity:Open",
      activity: { name: aOptions.name, data: aOptions.data }
    }).then((result) => {
      aCallback.handleEvent(Ci.nsIActivityUIGlueCallback.NATIVE_ACTIVITY, result);
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivitiesGlue]);
