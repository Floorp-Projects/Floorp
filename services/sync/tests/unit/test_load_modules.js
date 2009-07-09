const modules = [
                 "auth.js",
                 "base_records/collection.js",
                 "base_records/crypto.js",
                 "base_records/keys.js",
                 "base_records/wbo.js",
                 "constants.js",
                 "engines/bookmarks.js",
                 "engines/clientData.js",
                 "engines/cookies.js",
                 "engines/extensions.js",
                 "engines/forms.js",
                 "engines/history.js",
                 "engines/input.js",
                 "engines/microformats.js",
                 "engines/passwords.js",
                 "engines/plugins.js",
                 "engines/prefs.js",
                 "engines/tabs.js",
                 "engines/themes.js",
                 "engines.js",
                 "ext/Observers.js",
                 "ext/Preferences.js",
                 "faultTolerance.js",
                 "identity.js",
                 "log4moz.js",
                 "notifications.js",
                 "resource.js",
                 "service.js",
                 "stores.js",
                 "trackers.js",
                 "type_records/bookmark.js",
                 "type_records/clientData.js",
                 "type_records/forms.js",
                 "type_records/history.js",
                 "type_records/passwords.js",
                 "type_records/prefs.js",
                 "type_records/tabs.js",
                 "util.js",
];

function run_test() {
  for each (let m in modules) {
    dump("Attempting to load resource://weave/" + m + "\n");
    Components.utils.import("resource://weave/" + m, {});
  }
}

