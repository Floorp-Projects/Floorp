/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  onBeginUpdateBatch: function() {
  },
  onEndUpdateBatch: function() {
  },
  onVisit: function(aURI, aVisitID, aTime, aSessionId, aReferringId,
                    aTransitionType, _added)
  {
  },
  onTitleChanged: function(aURI, aPageTable)
  {
  },
  onBeforeDeleteURI: function(aURI)
  {
    this.removedURI = aURI;
  },
  onDeleteURI: function(aURI)
  {
    do_check_false(this.checked);
    do_check_true(this.removedURI.equals(aURI));
    this.checked = true;
  },
  onPageChanged: function(aURI, aWhat, aValue)
  {
  },
  onDeleteVisits: function()
  {
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavHistoryObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_removePage()
{
  // First we add the URI to history that we are going to remove.
  let testURI = uri("http://mozilla.org");
  hs.addVisit(testURI, Date.now() * 1000, null,
              Ci.nsINavHistoryService.TRANSITION_LINK, false, 0);

  // Add our observer, and remove it.
  let observer = new Observer();
  hs.addObserver(observer, false);
  hs.removePage(testURI);

  // Make sure we were notified!
  do_check_true(observer.checked);
  hs.removeObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
  test_removePage,
];
function run_test()
{
  tests.forEach(function(test) test());
}
