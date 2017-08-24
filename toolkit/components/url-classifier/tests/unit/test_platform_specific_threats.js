/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/AppConstants.jsm");

let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                 .getService(Ci.nsIUrlClassifierUtils);

function testMobileOnlyThreats() {
  // Mobile-only threat type(s):
  //   - goog-harmful-proto (POTENTIALLY_HARMFUL_APPLICATION)

  (function testUpdateRequest() {
    let requestWithPHA =
      urlUtils.makeUpdateRequestV4(["goog-phish-proto", "goog-harmful-proto"],
                                   ["AAAAAA", "AAAAAA"], 2);

    let requestNoPHA =
      urlUtils.makeUpdateRequestV4(["goog-phish-proto"], ["AAAAAA"], 1);

    if (AppConstants.platform === "android") {
      notEqual(requestWithPHA, requestNoPHA,
               "PHA (i.e. goog-harmful-proto) shouldn't be filtered on mobile platform.");
    } else {
      equal(requestWithPHA, requestNoPHA,
            "PHA (i.e. goog-harmful-proto) should be filtered on non-mobile platform.");
    }
  })();

  (function testFullHashRequest() {
    let requestWithPHA =
      urlUtils.makeFindFullHashRequestV4(["goog-phish-proto", "goog-harmful-proto"],
                                         ["", ""],       // state.
                                         [btoa("0123")], // prefix.
                                         2, 1);

    let requestNoPHA =
      urlUtils.makeFindFullHashRequestV4(["goog-phish-proto"],
                                         [""],           // state.
                                         [btoa("0123")], // prefix.
                                         1, 1);

    if (AppConstants.platform === "android") {
      notEqual(requestWithPHA, requestNoPHA,
               "PHA (i.e. goog-harmful-proto) shouldn't be filtered on mobile platform.");
    } else {
      equal(requestWithPHA, requestNoPHA,
            "PHA (i.e. goog-harmful-proto) should be filtered on non-mobile platform.");
    }
  })();
}

function testDesktopOnlyThreats() {
  // Desktop-only threats:
  //   - goog-downloadwhite-proto (CSD_WHITELIST)
  //   - goog-badbinurl-proto (MALICIOUS_BINARY)

  let requestWithDesktopOnlyThreats =
    urlUtils.makeUpdateRequestV4(["goog-phish-proto",
                                  "goog-downloadwhite-proto",
                                  "goog-badbinurl-proto"],
                                 ["", "", ""], 3);

  let requestNoDesktopOnlyThreats =
    urlUtils.makeUpdateRequestV4(["goog-phish-proto"], [""], 1);

  if (AppConstants.platform === "android") {
    equal(requestWithDesktopOnlyThreats, requestNoDesktopOnlyThreats,
          "Android shouldn't contain 'goog-downloadwhite-proto' and 'goog-badbinurl-proto'.");
  } else {
    notEqual(requestWithDesktopOnlyThreats, requestNoDesktopOnlyThreats,
             "Desktop should contain 'goog-downloadwhite-proto' and 'goog-badbinurl-proto'.");
  }
}

function run_test() {
  testMobileOnlyThreats();
  testDesktopOnlyThreats();
}
