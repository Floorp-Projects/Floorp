/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("browser.dom.window.dump.enabled", true);
pref("devtools.console.stdout.chrome", true);

pref("network.process.enabled", false);

pref("browser.cache.offline.enable", false);
pref("browser.cache.offline.storage.enable", false);
pref("browser.cache.disk.enable", false);
pref("permissions.memory_only", true);

// For testing only: used to test that backgroundtask-specific prefs are
// processed.  This just needs to be an unusual integer in the range 0..127.
pref("test.backgroundtask_specific_pref.exitCode", 79);
