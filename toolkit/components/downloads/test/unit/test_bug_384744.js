/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests Bug 384744 - or specifically that createInstance won't cause
// a failure with the download manager.  The old practice was bad because if
// someone called createInstance before anyone called getService, getService
// would fail, and we would have a non-functional download manager.

function run_test()
{
  var dmci = Cc["@mozilla.org/download-manager;1"].
             createInstance(Ci.nsIDownloadManager);
  var dmgs = Cc["@mozilla.org/download-manager;1"].
             getService(Ci.nsIDownloadManager);
  do_check_eq(dmci, dmgs);
}
