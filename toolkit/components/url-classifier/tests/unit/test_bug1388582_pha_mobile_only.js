Cu.import("resource://gre/modules/AppConstants.jsm");

let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                 .getService(Ci.nsIUrlClassifierUtils);

function testUpdateRequest() {
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
}

function testFullHashRequest() {
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
}

function run_test() {
  testUpdateRequest();
  testFullHashRequest();
}
