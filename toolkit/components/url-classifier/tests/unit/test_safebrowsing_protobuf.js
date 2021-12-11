function run_test() {
  let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"].getService(
    Ci.nsIUrlClassifierUtils
  );

  // No list at all.
  let requestNoList = urlUtils.makeUpdateRequestV4([], []);

  // Only one valid list name.
  let requestOneValid = urlUtils.makeUpdateRequestV4(
    ["goog-phish-proto"],
    ["AAAAAA"]
  );

  // Only one invalid list name.
  let requestOneInvalid = urlUtils.makeUpdateRequestV4(
    ["bad-list-name"],
    ["AAAAAA"]
  );

  // One valid and one invalid list name.
  let requestOneInvalidOneValid = urlUtils.makeUpdateRequestV4(
    ["goog-phish-proto", "bad-list-name"],
    ["AAAAAA", "AAAAAA"]
  );

  equal(requestNoList, requestOneInvalid);
  equal(requestOneValid, requestOneInvalidOneValid);
}
