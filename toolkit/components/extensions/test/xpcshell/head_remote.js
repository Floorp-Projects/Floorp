"use strict";

Services.prefs.setBoolPref("extensions.webextensions.remote", true);
Services.prefs.setIntPref("dom.ipc.keepProcessesAlive.extension", 1);

/* globals testEnv */
testEnv.expectRemote = true; // tested in head_test.js
