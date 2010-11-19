/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77bonardo.net> (Original Author)
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

/*
 * Tests failing cases of setAndLoadFaviconForPage
 */

const PB_KEEP_SESSION_PREF = "browser.privatebrowsing.keep_current_session";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "pb",
                                   "@mozilla.org/privatebrowsing;1",
                                   "nsIPrivateBrowsingService");

let favicons = [
  {
    uri: uri(do_get_file("favicon-normal16.png")),
    data: readFileData(do_get_file("favicon-normal16.png")),
    mimetype: "image/png",
    size: 286
  },
  {
    uri: uri(do_get_file("favicon-normal32.png")),
    data: readFileData(do_get_file("favicon-normal32.png")),
    mimetype: "image/png",
    size: 344
  },
];

let tests = [

  {
    desc: "test setAndLoadFaviconForPage for about: URIs",
    pageURI: uri("about:test1"),
    go: function go1() {

      PlacesUtils.favicons.setAndLoadFaviconForPage(this.pageURI, favicons[0].uri, true);
    },
    clean: function clean1() {}
  },

  {
    desc: "test setAndLoadFaviconForPage with history disabled",
    pageURI: uri("http://test2.bar/"),
    go: function go2() {
      // Temporarily disable history.
      Services.prefs.setBoolPref("places.history.enabled", false);

      PlacesUtils.favicons.setAndLoadFaviconForPage(this.pageURI, favicons[0].uri, true);
    },
    clean: function clean2() {
      Services.prefs.setBoolPref("places.history.enabled", true);
    }
  },

  {
    desc: "test setAndLoadFaviconForPage in PB mode for non-bookmarked URI",
    pageURI: uri("http://test3.bar/"),
    go: function go3() {
      if (!("@mozilla.org/privatebrowsing;1" in Cc))
        return;

      // Enable PB mode.
      Services.prefs.setBoolPref(PB_KEEP_SESSION_PREF, true);
      pb.privateBrowsingEnabled = true;

      PlacesUtils.favicons.setAndLoadFaviconForPage(this.pageURI, favicons[0].uri, true);
    },
    clean: function clean3() {
      if (!("@mozilla.org/privatebrowsing;1" in Cc))
        return;

      pb.privateBrowsingEnabled = false;
    }
  },

  { // This is a valid icon set test, that will cause the closing notification.
    desc: "test setAndLoadFaviconForPage for valid history uri",
    pageURI: uri("http://test4.bar/"),
    go: function go4() {
      PlacesUtils.favicons.setAndLoadFaviconForPage(this.pageURI, favicons[1].uri, true);
    },
    clean: function clean4() {}
  },

];

let historyObserver = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  onVisit: function() {},
  onTitleChanged: function() {},
  onBeforeDeleteURI: function() {},
  onDeleteURI: function() {},
  onClearHistory: function() {},
  onDeleteVisits: function() {},

  onPageChanged: function historyObserver_onPageChanged(pageURI, what, value) {
    if (what != Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON)
      return;

    // dump tables, useful if the test fails.
    dump_table("moz_places_temp");
    dump_table("moz_favicons");

    // Ensure we have been called by the last test.
    do_check_true(pageURI.equals(uri("http://test4.bar/")));

    // Ensure there is only one entry in favicons table.
    let stmt = DBConn().createStatement(
      "SELECT url FROM moz_favicons"
    );
    let c = 0;
    try {
      while (stmt.executeStep()) {
        do_check_eq(stmt.row.url, favicons[1].uri.spec);
        c++;
      }
    }
    finally {
      stmt.finalize();
    }
    do_check_eq(c, 1);

    do_test_finished();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
};

let currentTest = null;

function run_test() {
  do_test_pending();

  // check that the favicon loaded correctly
  do_check_eq(favicons[0].data.length, favicons[0].size);
  do_check_eq(favicons[1].data.length, favicons[1].size);

  // Observe history for onPageChanged notification.
  PlacesUtils.history.addObserver(historyObserver, false);

  // We run all the tests, then we wait for an onPageChanged notification,
  // ideally only the last test is successful, so it should be the only
  // notification we get.  Once we get it, test finishes.
  runNextTest();
};

function runNextTest() {
  if (currentTest) {
    currentTest.clean();
  }

  if (tests.length) {
    currentTest = tests.shift();
    print(currentTest.desc);
    currentTest.go();
    // Wait some time before calling the next test, this is needed to avoid
    // invoking clean() too early.  The first async step should run at least.
    do_timeout(500, runNextTest);
  }
}
