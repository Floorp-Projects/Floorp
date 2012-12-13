/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Added with bug 487511 to test that onBeforeDeleteURI is dispatched before
 * onDeleteURI and always with the right uri.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals and Constants

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

////////////////////////////////////////////////////////////////////////////////
//// Observer

function Observer()
{
}
Observer.prototype =
{
  checked: false,
  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onVisit: function () {},
  onTitleChanged: function () {},
  onBeforeDeleteURI: function (aURI, aGUID)
  {
    this.removedURI = aURI;
    this.removedGUID = aGUID;
    do_check_guid_for_uri(aURI, aGUID);
  },
  onDeleteURI: function (aURI, aGUID)
  {
    do_check_false(this.checked);
    do_check_true(this.removedURI.equals(aURI));
    do_check_eq(this.removedGUID, aGUID);
    this.checked = true;
  },
  onPageChanged: function () {},
  onDeleteVisits: function () {},
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  // First we add the URI to history that we are going to remove.
  let testURI = uri("http://mozilla.org");
  yield promiseAddVisits(testURI);

  // Add our observer, and remove it.
  let observer = new Observer();
  hs.addObserver(observer, false);
  hs.removePage(testURI);

  // Make sure we were notified!
  do_check_true(observer.checked);
  hs.removeObserver(observer);
});
