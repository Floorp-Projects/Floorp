"use strict";

module.exports = {
  "extends": [
    "../../../../testing/mochitest/mochitest.eslintrc.js"
  ],

  globals: {
    // Globals specific to mozmill
    "assert": false,
    "controller": false,
    "findElement": false,
    "mozmill": false,
    // Injected into tests via tps.jsm
    "Addons": false,
    "Bookmarks": false,
    "EnableEngines": false,
    "EnsureTracking": false,
    "Formdata": false,
    "History": false,
    "Login": false,
    "Passwords": false,
    "Phase": false,
    "Prefs": false,
    "RunMozmillTest": false,
    "STATE_DISABLED": false,
    "STATE_ENABLED": false,
    "Sync": false,
    "SYNC_WIPE_CLIENT": false,
    "SYNC_WIPE_REMOTE": false,
    "Tabs": false,
    "Windows": false,
    "WipeServer": false,
  }
};
