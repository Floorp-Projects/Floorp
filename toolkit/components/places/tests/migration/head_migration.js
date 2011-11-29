/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
let (commonFile = do_get_file("../head_common.js", false)) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

const kDBName = "places.sqlite";

/**
 * Sets the database to use for the given test.  This should be the very first
 * thing we do otherwise, this database will not be used!
 *
 * @param aFileName
 *        The filename of the database to use.  This database must exist in
 *        toolkit/components/places/tests/migration!
 */
function setPlacesDatabase(aFileName)
{
  let file = do_get_file(aFileName);

  // Ensure that our database doesn't already exist.
  let (dbFile = gProfD.clone()) {
    dbFile.append(kDBName);
    do_check_false(dbFile.exists());
  }

  file.copyToFollowingLinks(gProfD, kDBName);
}
