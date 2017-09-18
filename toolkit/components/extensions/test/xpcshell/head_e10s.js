"use strict";

/* globals ExtensionTestUtils */

// We want to force enable e10s so we can test remote extensions
Services.prefs.setBoolPref("browser.tabs.remote.force-enable", true);
// This causes nsIXULRuntime.browserTabsRemoteAutostart to reset and force e10s
Services.prefs.setStringPref("e10s.rollout.cohort", "xpcshell-test");

ExtensionTestUtils.remoteContentScripts = true;
