#filter dumbComments emptyLines substitution

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Non-static prefs that are specific to GeckoView belong in this file (unless
// there is a compelling and documented reason for them to belong in another
// file). Note that non-static prefs that are shared by GeckoView and Firefox
// for Android are in mobile.js, which this file includes.
//
// Please indent all prefs defined within #ifdef/#ifndef conditions. This
// improves readability, particular for conditional blocks that exceed a single
// screen.

#include mobile.js

pref("privacy.trackingprotection.pbmode.enabled", false);

pref("browser.tabs.remote.autostart", true);
pref("dom.ipc.keepProcessesAlive.web", 1);

pref("dom.ipc.processPrelaunch.enabled", false);

// Don't create the hidden window during startup.
pref("toolkit.lazyHiddenWindow", true);

pref("geckoview.console.enabled", false);

#ifdef RELEASE_OR_BETA
  pref("geckoview.logging", "Warn");
#else
  pref("geckoview.logging", "Debug");
#endif

// Enable WebShare support.
pref("dom.webshare.enabled", true);

// Enable capture attribute for file input.
pref("dom.capture.enabled", true);

// Disable Web Push until we get it working
pref("dom.push.enabled", true);

// enable external storage API
pref("dom.storageManager.enabled", true);

// enable storage access API
pref("dom.storage_access.enabled", true);

// Inherit locale from the OS, used for multi-locale builds
pref("intl.locale.requested", "");

// Enable Safe Browsing blocklist updates
pref("browser.safebrowsing.features.phishing.update", true);
pref("browser.safebrowsing.features.malware.update", true);

// Enable Tracking Protection blocklist updates
pref("browser.safebrowsing.features.trackingAnnotation.update", true);
pref("browser.safebrowsing.features.trackingProtection.update", true);

// Enable cryptomining protection blocklist updates
pref("browser.safebrowsing.features.cryptomining.update", true);
// Enable fingerprinting protection blocklist updates
pref("browser.safebrowsing.features.fingerprinting.update", true);

// Enable autoplay permission prompts
pref("media.geckoview.autoplay.request", true);

// Enable EME permission prompts
pref("media.eme.require-app-approval", true);

// Enable the Process Priority Manager
pref("dom.ipc.processPriorityManager.enabled", true);

pref("signon.debug", false);
pref("signon.showAutoCompleteFooter", true);
pref("security.insecure_field_warning.contextual.enabled", true);
pref("toolkit.autocomplete.delegate", true);

// Android doesn't support the new sync storage yet, we will have our own in
// Bug 1625257.
pref("webextensions.storage.sync.kinto", true);

// This value is derived from the calculation:
// MOZ_ANDROID_CONTENT_SERVICE_COUNT - dom.ipc.processCount
// (dom.ipc.processCount is set in GeckoRuntimeSettings.java)
#ifdef NIGHTLY_BUILD
  pref("dom.ipc.processCount.webCOOP+COEP", 38);
#endif

// Form autofill prefs.
pref("extensions.formautofill.addresses.capture.enabled", true);

// Debug prefs.
pref("browser.formfill.debug", false);
pref("extensions.formautofill.loglevel", "Warn");
