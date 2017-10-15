/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

const DB_FILENAME = "places.sqlite";

/**
 * Sets the database to use for the given test.  This should be the very first
 * thing in the test, otherwise this database will not be used!
 *
 * @param aFileName
 *        The filename of the database to use.  This database must exist in
 *        toolkit/components/places/tests/migration!
 * @return {Promise}
 */
var setupPlacesDatabase = async function(aFileName) {
  let currentDir = await OS.File.getCurrentDirectory();

  let src = OS.Path.join(currentDir, aFileName);
  Assert.ok((await OS.File.exists(src)), "Database file found");

  // Ensure that our database doesn't already exist.
  let dest = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  Assert.ok(!(await OS.File.exists(dest)), "Database file should not exist yet");

  await OS.File.copy(src, dest);
};

// This works provided all tests in this folder use add_task.
