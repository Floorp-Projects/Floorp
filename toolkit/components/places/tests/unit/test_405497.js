/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

/**
 * The callback object for runInBatchMode.
 *
 * @param aService
 *        Takes a reference to the history service or the bookmark service.
 *        This determines which service should be called when calling the second
 *        runInBatchMode the second time.
 */
function callback(aService)
{
  this.callCount = 0;
  this.service = aService;
}
callback.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsINavHistoryBatchCallback

  runBatched: function(aUserData)
  {
    this.callCount++;

    if (this.callCount == 1) {
      // We want to call run in batched once more.
      this.service.runInBatchMode(this, null);
      return;
    }

    do_check_eq(this.callCount, 2);
    do_test_finished();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryBatchCallback])
};

function run_test() {
  // checking the history service
  do_test_pending();
  hs.runInBatchMode(new callback(hs), null);

  // checking the bookmark service
  do_test_pending();
  bs.runInBatchMode(new callback(bs), null);
}
