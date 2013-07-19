/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is imported by each new webapp window but is only evaluated the first
 * time it is imported.  Put stuff here that you want to happen once on startup
 * before the webapp is loaded.  But note that the "stuff" happens immediately
 * the first time this module is imported.  So only put stuff here that must
 * happen before the webapp is loaded. */

this.EXPORTED_SYMBOLS = [];

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Initialize DOMApplicationRegistry by importing Webapps.jsm.
Cu.import("resource://gre/modules/Webapps.jsm");

// Initialize window-independent handling of webapps- notifications.
Cu.import("resource://webapprt/modules/WebappsHandler.jsm");
WebappsHandler.init();

// On firstrun, set permissions to their default values.
if (!Services.prefs.getBoolPref("webapprt.firstrun")) {
  // Once we support packaged apps, set their permissions here on firstrun.

  // Now that we've set the appropriate permissions, twiddle the firstrun
  // flag so we don't try to do so again.
  Services.prefs.setBoolPref("webapprt.firstrun", true);
}
