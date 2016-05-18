function run_test() {
  let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                   .getService(Ci.nsIUrlClassifierUtils);

  // The google protocol version should be "2.2" until we enable SB v4
  // by default.
  equal(urlUtils.getProtocolVersion("google"), "2.2");

  // Mozilla protocol version will stick to "2.2".
  equal(urlUtils.getProtocolVersion("mozilla"), "2.2");

  // Unknown provider version will be "2.2".
  equal(urlUtils.getProtocolVersion("unknown-provider"), "2.2");
}