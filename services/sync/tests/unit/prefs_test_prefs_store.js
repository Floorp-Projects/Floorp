// This is a "preferences" file used by test_prefs_store.js

/* global pref, user_pref */

// The prefs that control what should be synced.
// Most of these are "default" prefs, so the value itself will not sync.
pref("services.sync.prefs.sync.testing.int", true);
pref("services.sync.prefs.sync.testing.string", true);
pref("services.sync.prefs.sync.testing.bool", true);
pref("services.sync.prefs.sync.testing.dont.change", true);
// this one is a user pref, so it *will* sync.
user_pref("services.sync.prefs.sync.testing.turned.off", false);
pref("services.sync.prefs.sync.testing.nonexistent", true);
pref("services.sync.prefs.sync.testing.default", true);
pref("services.sync.prefs.sync.testing.synced.url", true);
// We shouldn't sync the URL, or the flag that says we should sync the pref
// (otherwise some other client might overwrite our local value).
user_pref("services.sync.prefs.sync.testing.unsynced.url", true);

// The preference values - these are all user_prefs, otherwise their value
// will not be synced.
user_pref("testing.int", 123);
user_pref("testing.string", "ohai");
user_pref("testing.bool", true);
user_pref("testing.dont.change", "Please don't change me.");
user_pref("testing.turned.off", "I won't get synced.");
user_pref("testing.not.turned.on", "I won't get synced either!");
// Some url we don't want to sync
user_pref("testing.unsynced.url", "moz-extension://d5d31b00-b944-4afb-bd3d-d0326551a0ae");
user_pref("testing.synced.url", "https://www.example.com");

// A pref that exists but still has the default value - will be synced with
// null as the value.
pref("testing.default", "I'm the default value");

// A pref that shouldn't be synced
