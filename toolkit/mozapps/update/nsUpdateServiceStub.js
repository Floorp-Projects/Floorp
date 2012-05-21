/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");

const Ci = Components.interfaces;

const DIR_UPDATES         = "updates";
const FILE_UPDATE_STATUS  = "update.status";

const KEY_APPDIR          = "XCurProcD";

#ifdef XP_WIN
#define USE_UPDROOT
#elifdef ANDROID
#define USE_UPDROOT
#endif

#ifdef USE_UPDROOT
const KEY_UPDROOT         = "UpdRootD";
#endif

/**
#  Gets the specified directory at the specified hierarchy under the update root
#  directory without creating it if it doesn't exist.
#  @param   pathArray
#           An array of path components to locate beneath the directory
#           specified by |key|
#  @return  nsIFile object for the location specified.
 */
function getUpdateDirNoCreate(pathArray) {
#ifdef USE_UPDROOT
  try {
    let dir = FileUtils.getDir(KEY_UPDROOT, pathArray, false);
    return dir;
  } catch (e) {
  }
#endif
  return FileUtils.getDir(KEY_APPDIR, pathArray, false);
}

function UpdateServiceStub() {
  let statusFile = getUpdateDirNoCreate([DIR_UPDATES, "0"]);
  statusFile.append(FILE_UPDATE_STATUS);
  // If the update.status file exists then initiate post update processing.
  if (statusFile.exists()) {
    let aus = Components.classes["@mozilla.org/updates/update-service;1"].
              getService(Ci.nsIApplicationUpdateService).
              QueryInterface(Ci.nsIObserver);
    aus.observe(null, "post-update-processing", "");
  }
}
UpdateServiceStub.prototype = {
  observe: function(){},
  classID: Components.ID("{e43b0010-04ba-4da6-b523-1f92580bc150}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([UpdateServiceStub]);
