"use strict";

/* globals ExtensionTestUtils */

// xpcshell disables e10s by default. Turn it on.
Services.prefs.setBoolPref("browser.tabs.remote.autostart", true);

ExtensionTestUtils.remoteContentScripts = true;
