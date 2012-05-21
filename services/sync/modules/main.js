/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ['Weave'];

let Weave = {};
Components.utils.import("resource://services-sync/constants.js", Weave);
let lazies = {
  "record.js":            ["CollectionKeys"],
  "engines.js":           ['Engines', 'Engine', 'SyncEngine', 'Store'],
  "engines/addons.js":    ["AddonsEngine"],
  "engines/bookmarks.js": ['BookmarksEngine', 'BookmarksSharingManager'],
  "engines/clients.js":   ["Clients"],
  "engines/forms.js":     ["FormEngine"],
  "engines/history.js":   ["HistoryEngine"],
  "engines/prefs.js":     ["PrefsEngine"],
  "engines/passwords.js": ["PasswordEngine"],
  "engines/tabs.js":      ["TabEngine"],
  "engines/apps.js":      ["AppsEngine"],
  "identity.js":          ["Identity"],
  "jpakeclient.js":       ["JPAKEClient"],
  "keys.js":              ["BulkKeyBundle", "SyncKeyBundle"],
  "notifications.js":     ["Notifications", "Notification", "NotificationButton"],
  "policies.js":          ["SyncScheduler", "ErrorHandler",
                           "SendCredentialsController"],
  "resource.js":          ["Resource", "AsyncResource"],
  "service.js":           ["Service"],
  "status.js":            ["Status"],
  "util.js":              ['Utils', 'Svc', 'Str']
};

function lazyImport(module, dest, props) {
  function getter(prop) function() {
    let ns = {};
    Components.utils.import(module, ns);
    delete dest[prop];
    return dest[prop] = ns[prop];
  };
  props.forEach(function(prop) dest.__defineGetter__(prop, getter(prop)));
}

for (let mod in lazies) {
  lazyImport("resource://services-sync/" + mod, Weave, lazies[mod]);
}
