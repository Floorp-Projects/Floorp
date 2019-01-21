/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

#include mobile.js

pref("privacy.trackingprotection.pbmode.enabled", false);

pref("dom.ipc.keepProcessesAlive.web", 1);
pref("dom.ipc.processCount", 1);
pref("dom.ipc.processHangMonitor", true);
pref("dom.ipc.processPrelaunch.enabled", false);

// Tell Telemetry that we're in GeckoView mode.
pref("toolkit.telemetry.isGeckoViewMode", true);
// Disable the Telemetry Event Ping
pref("toolkit.telemetry.eventping.enabled", false);

pref("geckoview.console.enabled", false);

#ifdef RELEASE_OR_BETA
pref("geckoview.logging", "Warn");
#else
pref("geckoview.logging", "Debug");
#endif

// Disable Web Push until we get it working
pref("dom.push.enabled", false);

// Unlike Fennec, GeckoView may have WebRender enabled, and with WebRender we're
// going with containerless scrolling (because there are no layers at all with
// WR, so why not go containerless). So we set this pref to pick up the value
// in gfxPrefs.h from whether or not WR is enabled.
pref("layout.scroll.root-frame-containers", 2);

// Scroll anchoring is disabled in Fennec but not GeckoView because of
// regressions involving the dynamic toolbar. See bug 1520666 for more details.
pref("layout.css.scroll-anchoring.enabled", true);

// Inherit locale from the OS, used for multi-locale builds
pref("intl.locale.requested", "");
