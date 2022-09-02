/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These preferences override override browser/-specific preferences in
// `firefox.js`.  Use `backgroundtasks.js` to override general Gecko preferences
// in `greprefs.js`.

// XUL notifications make no sense in background tasks.  This is only applies to
// Windows for now.
pref("alerts.useSystemBackend", true);
pref("alerts.useSystemBackend.windows.notificationserver.enabled", true);

// Configure Messaging Experiments for background tasks, with
// background task-specific feature ID.  The regular Firefox Desktop
// Remote Settings collection will be used.
pref("browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments", "{\"id\":\"messaging-experiments\",\"enabled\":true,\"type\":\"remote-experiments\",\"featureIds\":[\"backgroundTaskMessage\"],\"updateCycleInMs\":3600000}");

// Disable all other Messaging System providers save for
// `browser.newtabpage.activity-stream.asrouter.providers.message-groups`, which
// is required for the system to function.
pref("browser.newtabpage.activity-stream.asrouter.providers.cfr", "null");
pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "null");
pref("browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel", "null");

// The `browser.newtabpage.activity-stream.asrouter.providers.cfr` provider is
// disabled, but belt and braces: disable extension recommendations and feature
// recommendations.  Neither of these make sense in background tasks, and they
// could trigger telemetry.
pref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons", false);
pref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features", false);

// Don't refresh experiments while a background task is running.
pref("app.normandy.run_interval_seconds", 0);

// Use a separate Nimbus application ID from regular Firefox Desktop.
// This prevents enrolling in regular desktop experiments.
pref("nimbus.appId", "firefox-desktop-background-task");
