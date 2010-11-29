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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Gavin Sharp <gavin@gavinsharp.com> (original author)
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
 * Tests for nsIFaviconService::SetAndLoadFaviconForPage()
 */

var iconsvc = PlacesUtils.favicons;

function addBookmark(aURI) {
  var bs = PlacesUtils.bookmarks;
  return bs.insertBookmark(bs.unfiledBookmarksFolder, aURI,
                           bs.DEFAULT_INDEX, aURI.spec);
}

function checkAddSucceeded(pageURI, mimetype, data) {
  var savedFaviconURI = iconsvc.getFaviconForPage(pageURI);
  var outMimeType = {};
  var outData = iconsvc.getFaviconData(savedFaviconURI, outMimeType, {});

  // Ensure input and output are identical
  do_check_eq(mimetype, outMimeType.value);
  do_check_true(compareArrays(data, outData));
}

var favicons = [
  {
    uri: uri(do_get_file("favicon-normal32.png")),
    data: readFileData(do_get_file("favicon-normal32.png")),
    mimetype: "image/png"
  }
];

var tests = [
  {
    desc: "test setAndLoadFaviconForPage for valid history uri",
    pageURI: uri("http://test1.bar/"),
    go: function go1() {
      this.favicon = favicons[0];

      iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);
    },
    check: function check1() {
      checkAddSucceeded(this.pageURI, this.favicon.mimetype, this.favicon.data);
    }
  },

  {
    desc: "test setAndLoadFaviconForPage for bookmarked about: URIs",
    pageURI: uri("about:test2"),
    favicon: favicons[0],
    go: function go3() {
      // Add as bookmark
      addBookmark(this.pageURI);

      iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);
    },
    check: function check3() {
      checkAddSucceeded(this.pageURI, this.favicon.mimetype, this.favicon.data);
    }
  },

  {
    desc: "test setAndLoadFaviconForPage with history disabled for bookmarked URI",
    pageURI: uri("http://test3.bar/"),
    favicon: favicons[0],
    go: function go5() {
      // disable history
      var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      prefs.setBoolPref("places.history.enabled", false);

      // Add as bookmark
      addBookmark(this.pageURI);

      iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);

      prefs.setBoolPref("places.history.enabled", true);
    },
    check: function check5() {
      checkAddSucceeded(this.pageURI, this.favicon.mimetype, this.favicon.data);
    }
  },

];

if ("@mozilla.org/privatebrowsing;1" in Cc) {
  tests.push({
    desc: "test setAndLoadFaviconForPage in PB mode for bookmarked URI",
    pageURI: uri("http://test4.bar/"),
    favicon: favicons[0],
    go: function go6() {
      try {
      // enable PB
      var pb = Cc["@mozilla.org/privatebrowsing;1"].
               getService(Ci.nsIPrivateBrowsingService);
      var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
      pb.privateBrowsingEnabled = true;

      // Add as bookmark
      addBookmark(this.pageURI);

      iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);

      pb.privateBrowsingEnabled = false;
      prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
      } catch (ex) {do_throw("ex: " + ex); }
    },
    check: function check6() {
      checkAddSucceeded(this.pageURI, this.favicon.mimetype, this.favicon.data);
    }
  });
}
var historyObserver = {
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

    if (pageURI.equals(tests[currentTestIndex].pageURI)) {
      tests[currentTestIndex].check();
      currentTestIndex++;
      if (currentTestIndex == tests.length)
        do_test_finished();
      else
        tests[currentTestIndex].go();
    }
    else
      do_throw("Received PageChanged for a non-current test!");
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavHistoryObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var currentTestIndex = 0;
function run_test() {
  do_test_pending();

  // check that the favicon loaded correctly
  do_check_eq(favicons[0].data.length, 344);

  PlacesUtils.history.addObserver(historyObserver, false);

  // Start the tests
  tests[currentTestIndex].go();
};
