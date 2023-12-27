/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution;

// These preferences override Gecko preferences in `greprefs.js`.  Use
// `backgroundtasks_browser.js` to override browser/-specific preferences in
// `firefox.js`.

/* global pref */

pref("browser.dom.window.dump.enabled", true);
pref("devtools.console.stdout.chrome", true);

pref("browser.cache.disk.enable", false);
pref("permissions.memory_only", true);

// For testing only: used to test that backgroundtask-specific prefs are
// processed.  This just needs to be an unusual integer in the range 0..127.
pref("test.backgroundtask_specific_pref.exitCode", 79);

// Enable the browser toolbox by default.  The browser toolbox is available only
// when launching the background task with `--jsdebugger` on the command line,
// and an attacker who can launch background task processes with arbitrary
// parameters and execution environment can already access this functionality,
// so there's no need to restrict access via preferences.
pref("devtools.chrome.enabled", true);
pref("devtools.debugger.remote-enabled", true);
pref("devtools.debugger.prompt-connection", false);

// Background tasks do not persist the cookie database: they should
// not be using cookies for network requests.
pref("network.cookie.noPersistentStorage", true);

// Background tasks don't need to worry about perceived performance. We disable
// fast shutdown to reduce the risk of open file handles preventing cleanup of
// the ephemeral profile directory.
pref("toolkit.shutdown.fastShutdownStage", 0);

// Avoid a race between initializing font lists and rapid shutdown,
// particularly on macOS.  Compare Bug 1777332.
pref("gfx.font-list-omt.enabled", false);

// Prevent key#.db and cert#.db from being created in the ephemeral profile.
pref("security.nocertdb", true);

// Prevent asynchronous preference writes.
pref("preferences.allow.omt-write", false);

// Enable automatic restarts during background updates for Nightly builds.
#ifdef NIGHTLY_BUILD
pref("app.update.background.automaticRestartEnabled", true);
#else
pref("app.update.background.automaticRestartEnabled", false);
#endif

pref("defaultAgent.cppFallback.enabled", false);
