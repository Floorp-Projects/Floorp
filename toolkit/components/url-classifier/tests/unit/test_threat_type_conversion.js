function run_test() {
  let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"].getService(
    Ci.nsIUrlClassifierUtils
  );

  // Test list name to threat type conversion.

  equal(urlUtils.convertListNameToThreatType("goog-malware-proto"), 1);
  equal(urlUtils.convertListNameToThreatType("googpub-phish-proto"), 2);
  equal(urlUtils.convertListNameToThreatType("goog-unwanted-proto"), 3);
  equal(urlUtils.convertListNameToThreatType("goog-harmful-proto"), 4);
  equal(urlUtils.convertListNameToThreatType("goog-phish-proto"), 5);
  equal(urlUtils.convertListNameToThreatType("goog-badbinurl-proto"), 7);
  equal(urlUtils.convertListNameToThreatType("goog-downloadwhite-proto"), 9);

  try {
    urlUtils.convertListNameToThreatType("bad-list-name");
    ok(false, "Bad list name should lead to exception.");
  } catch (e) {}

  try {
    urlUtils.convertListNameToThreatType("bad-list-name");
    ok(false, "Bad list name should lead to exception.");
  } catch (e) {}

  // Test threat type to list name conversion.
  equal(urlUtils.convertThreatTypeToListNames(1), "goog-malware-proto");
  equal(
    urlUtils.convertThreatTypeToListNames(2),
    "googpub-phish-proto,moztest-phish-proto,test-phish-proto"
  );
  equal(
    urlUtils.convertThreatTypeToListNames(3),
    "goog-unwanted-proto,moztest-unwanted-proto,test-unwanted-proto"
  );
  equal(urlUtils.convertThreatTypeToListNames(4), "goog-harmful-proto");
  equal(urlUtils.convertThreatTypeToListNames(5), "goog-phish-proto");
  equal(urlUtils.convertThreatTypeToListNames(7), "goog-badbinurl-proto");
  equal(urlUtils.convertThreatTypeToListNames(9), "goog-downloadwhite-proto");

  try {
    urlUtils.convertThreatTypeToListNames(0);
    ok(false, "Bad threat type should lead to exception.");
  } catch (e) {}

  try {
    urlUtils.convertThreatTypeToListNames(100);
    ok(false, "Bad threat type should lead to exception.");
  } catch (e) {}
}
