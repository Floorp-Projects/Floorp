// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["PermissionsUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");


let gImportedPrefBranches = new Set();

function importPrefBranch(aPrefBranch, aPermission, aAction) {
  let list = Services.prefs.getChildList(aPrefBranch, {});

  for (let pref of list) {
    let hosts = "";
    try {
      hosts = Services.prefs.getCharPref(pref);
    } catch (e) {}

    if (!hosts)
      continue;

    hosts = hosts.split(",");

    for (let host of hosts) {
      let uri = null;
      try {
        uri = Services.io.newURI("http://" + host, null, null);
      } catch (e) {
        try {
          uri = Services.io.newURI(host, null, null);
        } catch (e2) {}
      }

      try {
        Services.perms.add(uri, aPermission, aAction);
      } catch (e) {}
    }

    Services.prefs.setCharPref(pref, "");
  }
}


this.PermissionsUtils = {
  /**
   * Import permissions from perferences to the Permissions Manager. After being
   * imported, all processed permissions will be set to an empty string.
   * Perferences are only processed once during the application's
   * lifetime - it's safe to call this multiple times without worrying about
   * doing unnecessary work, as the preferences branch will only be processed
   * the first time.
   *
   * @param aPrefBranch  Preferences branch to import from. The preferences
   *                     under this branch can specify whitelist (ALLOW_ACTION)
   *                     or blacklist (DENY_ACTION) additions using perference
   *                     names of the form:
   *                     * <BRANCH>.whitelist.add.<ID>
   *                     * <BRANCH>.blacklist.add.<ID>
   *                     Where <ID> can be any valid preference name.
   *                     The value is expected to be a comma separated list of
   *                     host named. eg:
   *                     * something.example.com
   *                     * foo.exmaple.com,bar.example.com
   *
   * @param aPermission Permission name to be passsed to the Permissions
   *                    Manager.
   */
  importFromPrefs: function(aPrefBranch, aPermission) {
    if (!aPrefBranch.endsWith("."))
      aPrefBranch += ".";

    // Ensure we only import this pref branch once.
    if (gImportedPrefBranches.has(aPrefBranch))
     return;

    importPrefBranch(aPrefBranch + "whitelist.add", aPermission,
                     Services.perms.ALLOW_ACTION);
    importPrefBranch(aPrefBranch + "blacklist.add", aPermission,
                     Services.perms.DENY_ACTION);

    gImportedPrefBranches.add(aPrefBranch);
  }
};
