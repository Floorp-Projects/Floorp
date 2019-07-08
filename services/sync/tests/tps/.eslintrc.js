"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/mochitest-test"
  ],

  globals: {
    // Injected into tests via tps.jsm
    "Addons": false,
    "Addresses": false,
    "Bookmarks": false,
    "CreditCards": false,
    "EnableEngines": false,
    "EnsureTracking": false,
    "Formdata": false,
    "History": false,
    "Login": false,
    "Passwords": false,
    "Phase": false,
    "Prefs": false,
    "STATE_DISABLED": false,
    "STATE_ENABLED": false,
    "Sync": false,
    "SYNC_WIPE_CLIENT": false,
    "SYNC_WIPE_REMOTE": false,
    "Tabs": false,
    "Windows": false,
    "WipeServer": false,
  },
};
