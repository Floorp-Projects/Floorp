/*
 * Tests for nsIFaviconService
 */

var iconsvc = Cc["@mozilla.org/browser/favicon-service;1"].
              getService(Ci.nsIFaviconService);

function addBookmark(aURI) {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var bmId = bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                                  aURI,
                                  bmsvc.DEFAULT_INDEX,
                                  aURI.spec);
}

function checkAddSucceeded(pageURI, mimetype, data) {
  var savedFaviconURI = iconsvc.getFaviconForPage(pageURI);
  var outMimeType = {};
  var outData = iconsvc.getFaviconData(savedFaviconURI, outMimeType, {});

  // Ensure input and output are identical
  do_check_eq(mimetype, outMimeType.value);
  do_check_true(compareArrays(data, outData));
}

// TODO bug 527036: There is no way to know whether setting a favicon failed.
//function checkAddFailed(pageURI) {
//  var savedFaviconURI = null;
//  try {
//    savedFaviconURI = iconsvc.getFaviconForPage(pageURI);
//  } catch (ex) {}
//
//  // ensure that the addition failed
//  do_check_eq(savedFaviconURI, null);
//}

var favicons = [
  {
    uri: iosvc.newFileURI(do_get_file("favicon-normal32.png")),
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

  // TODO bug 527036: There is no way to know whether setting a favicon failed.
  //{
  //  desc: "test setAndLoadFaviconForPage for about: URIs",
  //  pageURI: uri("about:test1"),
  //  go: function go2() {
  //    iconsvc.setAndLoadFaviconForPage(this.pageURI, favicons[0].uri, true);
  //  },
  //  check: function check2() {
  //    checkAddFailed(this.pageURI);
  //  }
  //},

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

  // TODO bug 527036: There is no way to know whether setting a favicon failed.
  //{
  //  desc: "test setAndLoadFaviconForPage with history disabled",
  //  pageURI: uri("http://test2.bar/"),
  //  go: function go4() {
  //    // disable history
  //    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  //    prefs.setIntPref("browser.history_expire_days", 0);
  //
  //    iconsvc.setAndLoadFaviconForPage(this.pageURI, favicons[0].uri, true);
  //
  //    try {
  //      prefs.clearUserPref("browser.history_expire_days");
  //    } catch (ex) {}
  //  },
  //  check: function check4() {
  //    checkAddFailed(this.pageURI);
  //  }
  //},

  {
    desc: "test setAndLoadFaviconForPage with history disabled for bookmarked URI",
    pageURI: uri("http://test3.bar/"),
    favicon: favicons[0],
    go: function go5() {
      // disable history
      var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      prefs.setIntPref("browser.history_expire_days", 0);

      // Add as bookmark
      addBookmark(this.pageURI);

      iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);

      try {
        prefs.clearUserPref("browser.history_expire_days");
      } catch (ex) {}
    },
    check: function check5() {
      checkAddSucceeded(this.pageURI, this.favicon.mimetype, this.favicon.data);
    }
  },

  // TODO bug 527036: There is no way to know whether setting a favicon failed.
  //{
  //  desc: "test setAndLoadFaviconForPage in PB mode for non-bookmarked URI",
  //  pageURI: uri("http://test5.bar/"),
  //  favicon: favicons[0],
  //  go: function go7() {
  //    // enable PB
  //    var pb = Cc["@mozilla.org/privatebrowsing;1"].
  //             getService(Ci.nsIPrivateBrowsingService);
  //    pb.privateBrowsingEnabled = true;
  //
  //    iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);
  //
  //    pb.privateBrowsingEnabled = false;
  //  },
  //  check: function check7() {
  //    checkAddFailed(this.pageURI);
  //  }
  //},
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
      pb.privateBrowsingEnabled = true;

      // Add as bookmark
      addBookmark(this.pageURI);

      iconsvc.setAndLoadFaviconForPage(this.pageURI, this.favicon.uri, true);

      pb.privateBrowsingEnabled = false;
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
  onPageExpired: function() {},

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

  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  hs.addObserver(historyObserver, false);

  // Start the tests
  tests[currentTestIndex].go();
};
