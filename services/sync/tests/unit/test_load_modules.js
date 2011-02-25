const modules = [
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
                 "record.js",
                 "resource.js",
                 "service.js",
                 "util.js",
];

function run_test() {
  for each (let m in modules) {
    _("Attempting to load resource://services-sync/" + m);
    Cu.import("resource://services-sync/" + m, {});
  }
}

