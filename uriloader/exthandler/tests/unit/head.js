/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Initialization for tests related to invoking external handler applications.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://testing-common/HandlerServiceTestUtils.jsm", this);
Cu.import("resource://testing-common/TestUtils.jsm");

HandlerServiceTestUtils.Assert = Assert;

do_get_profile();

let jsonPath = OS.Path.join(OS.Constants.Path.profileDir, "handlers.json");

let rdfFile = FileUtils.getFile("ProfD", ["mimeTypes.rdf"]);

function deleteDatasourceFile() {
  if (rdfFile.exists()) {
    rdfFile.remove(false);
  }
}

// Delete the existing datasource file, if any, so we start from scratch.
// We also do this after finishing the tests, so there shouldn't be an old
// file lying around, but just in case we delete it here as well.
deleteDatasourceFile();
do_register_cleanup(deleteDatasourceFile);
