function run_test() {
  var idnService = Cc["@mozilla.org/network/idn-service;1"]
                   .getService(Ci.nsIIDNService);

  var isASCII = {};
  Assert.equal(idnService.convertToDisplayIDN("xn--", isASCII), "xn--");
}
