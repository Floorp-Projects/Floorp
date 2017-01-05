/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../dom/push/test/xpcshell/head.js */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var profileDir = do_get_profile();

/**
 * Removes any files that could make our tests fail.
 */
function cleanUp() {
  let files = [
    "places.sqlite",
    "cookies.sqlite",
    "signons.sqlite",
    "permissions.sqlite"
  ];

  for (let i = 0; i < files.length; i++) {
    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append(files[i]);
    if (file.exists())
      file.remove(false);
  }
}
cleanUp();
