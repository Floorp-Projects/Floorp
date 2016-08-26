function run_test() {
  let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                   .getService(Ci.nsIUrlClassifierUtils);

  // Test list name to threat type conversion.

  equal(urlUtils.convertListNameToThreatType("goog-malware-proto"), 1);
  equal(urlUtils.convertListNameToThreatType("googpub-phish-proto"), 2);
  equal(urlUtils.convertListNameToThreatType("goog-unwanted-proto"), 3);
  equal(urlUtils.convertListNameToThreatType("goog-phish-proto"), 5);

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
  equal(urlUtils.convertThreatTypeToListNames(2), "googpub-phish-proto,test-phish-proto");
  equal(urlUtils.convertThreatTypeToListNames(3), "goog-unwanted-proto");
  equal(urlUtils.convertThreatTypeToListNames(5), "goog-phish-proto");

  try {
    urlUtils.convertThreatTypeToListNames(0);
    ok(false, "Bad threat type should lead to exception.");
  } catch (e) {}

  try {
    urlUtils.convertThreatTypeToListNames(100);
    ok(false, "Bad threat type should lead to exception.");
  } catch (e) {}
}