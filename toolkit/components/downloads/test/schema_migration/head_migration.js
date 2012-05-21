/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

do_get_profile();

do_register_cleanup(function() {
  Services.obs.notifyObservers(null, "quit-application", null);
});

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

function importDatabaseFile(aFName)
{
  var file = do_get_file(aFName);
  var newFile = dirSvc.get("ProfD", Ci.nsIFile);
  file.copyTo(newFile, "downloads.sqlite");
}

function cleanup()
{
  // removing database
  var dbFile = dirSvc.get("ProfD", Ci.nsIFile);
  dbFile.append("downloads.sqlite");
  if (dbFile.exists())
    try { dbFile.remove(true); } catch(e) { /* stupid windows box */ }
}

cleanup();

