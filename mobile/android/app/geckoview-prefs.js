/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

#include mobile.js

pref("privacy.trackingprotection.pbmode.enabled", false);
pref("dom.ipc.processCount", 1);
pref("dom.ipc.keepProcessesAlive.web", 1);
pref("dom.ipc.processPrelaunch.enabled", false);

// Tell Telemetry that we're in GeckoView mode.
pref("toolkit.telemetry.isGeckoViewMode", true);
// Disable the Telemetry Event Ping
pref("toolkit.telemetry.eventping.enabled", false);

pref("geckoview.console.enabled", false);

