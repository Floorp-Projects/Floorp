function run_test() {
  let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                   .getService(Ci.nsIUrlClassifierUtils);

  // Test list name to threat type conversion.

  equal(urlUtils.convertListNameToThreatType("goog-malware-proto"), 1);
  equal(urlUtils.convertListNameToThreatType("goog-phish-proto"), 2);
  equal(urlUtils.convertListNameToThreatType("goog-unwanted-proto"), 3);

  try {
    urlUtils.convertListNameToThreatType("bad-list-name");
    ok(false, "Bad list name should lead to exception.");
  } catch (e) {}

  try {
    urlUtils.convertListNameToThreatType("bad-list-name");
    ok(false, "Bad list name should lead to exception.");
  } catch (e) {}

  // Test threat type to list name conversion.
  equal(urlUtils.convertThreatTypeToListName(1), "goog-malware-proto");
  equal(urlUtils.convertThreatTypeToListName(2), "goog-phish-proto");
  equal(urlUtils.convertThreatTypeToListName(3), "goog-unwanted-proto");

  try {
    urlUtils.convertThreatTypeToListName(0);
    ok(false, "Bad threat type should lead to exception.");
  } catch (e) {}

  try {
    urlUtils.convertThreatTypeToListName(100);
    ok(false, "Bad threat type should lead to exception.");
  } catch (e) {}
}