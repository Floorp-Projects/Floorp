const modules = [
                 "auth.js",
                 "base_records/collection.js",
                 "base_records/crypto.js",
                 "base_records/keys.js",
                 "base_records/wbo.js",
                 "constants.js",
                 "engines/bookmarks.js",
                 "engines/clients.js",
                 "engines/forms.js",
                 "engines/history.js",
                 "engines/passwords.js",
                 "engines/prefs.js",
                 "engines/tabs.js",
                 "engines.js",
                 "ext/Observers.js",
                 "ext/Preferences.js",
                 "identity.js",
                 "log4moz.js",
                 "main.js",
                 "notifications.js",
                 "resource.js",
                 "service.js",
                 "stores.js",
                 "trackers.js",
                 "type_records/bookmark.js",
                 "type_records/clients.js",
                 "type_records/forms.js",
                 "type_records/history.js",
                 "type_records/passwords.js",
                 "type_records/prefs.js",
                 "type_records/tabs.js",
                 "util.js",
];

function run_test() {
  for each (let m in modules) {
    _("Attempting to load resource://services-sync/" + m);
    Cu.import("resource://services-sync/" + m, {});
  }
}

