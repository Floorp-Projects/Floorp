/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["Service"];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://services-common/preferences.js");

const PREFS_BRANCH = "services.notifications.";


function NotificationSvc() {
  this.ready = false;
  this.prefs = new Preferences(PREFS_BRANCH);
}
NotificationSvc.prototype = {

  get serverURL() this.prefs.get("serverURL"),

  onStartup: function onStartup() {
    this.ready = true;
  }
};

let Service = new NotificationSvc();
Service.onStartup();
