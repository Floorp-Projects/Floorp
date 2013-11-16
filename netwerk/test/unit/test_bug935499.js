function run_test() {
  var idnService = Cc["@mozilla.org/network/idn-service;1"]
                   .getService(Ci.nsIIDNService);

  var isASCII = {};
  do_check_eq(idnService.convertToDisplayIDN("xn--", isASCII), "xn--");
}
