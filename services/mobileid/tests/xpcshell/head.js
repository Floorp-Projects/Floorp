/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

"use strict";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

(function initMobileIdTestingInfrastructure() {
  do_get_profile();

  Services.prefs.setCharPref("services.mobileid.loglevel", "Debug");

}).call(this);
