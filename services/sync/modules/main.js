/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Sync
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['Weave'];

let Weave = {};
Components.utils.import("resource://services-sync/constants.js", Weave);
let lazies = {
  "auth.js":              ['Auth', 'BrokenBasicAuthenticator',
                           'BasicAuthenticator', 'NoOpAuthenticator'],
  "base_records/keys.js": ['PubKey', 'PrivKey', 'PubKeys', 'PrivKeys'],
  "engines.js":           ['Engines', 'Engine', 'SyncEngine'],
  "engines/bookmarks.js": ['BookmarksEngine', 'BookmarksSharingManager'],
  "engines/clients.js":   ["Clients"],
  "engines/forms.js":     ["FormEngine"],
  "engines/history.js":   ["HistoryEngine"],
  "engines/prefs.js":     ["PrefsEngine"],
  "engines/passwords.js": ["PasswordEngine"],
  "engines/tabs.js":      ["TabEngine"],
  "identity.js":          ["Identity", "ID"],
  "notifications.js":     ["Notifications", "Notification", "NotificationButton"],
  "resource.js":          ["Resource"],
  "service.js":           ["Service"],
  "status.js":            ["Status"],
  "stores.js":            ["Store"],
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
