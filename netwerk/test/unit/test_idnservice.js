// Tests nsIIDNService

var reference = [
                 // The 3rd element indicates whether the second element
                 // is ACE-encoded
                 ["asciihost", "asciihost", false],
                 ["b\u00FCcher", "xn--bcher-kva", true]
                ];

function run_test() {
  var idnService = Components.classes["@mozilla.org/network/idn-service;1"]
                             .getService(Components.interfaces.nsIIDNService);

  for (var i = 0; i < reference.length; ++i) {
     dump("Testing " + reference[i] + "\n");
     // We test the following:
     // - Converting UTF-8 to ACE and back gives us the expected answer
     // - Converting the ASCII string UTF-8 -> ACE leaves the string unchanged
     // - isACE returns true when we expect it to (third array elem true)
     do_check_eq(idnService.convertUTF8toACE(reference[i][0]), reference[i][1]);
     do_check_eq(idnService.convertUTF8toACE(reference[i][1]), reference[i][1]);
     do_check_eq(idnService.convertACEtoUTF8(reference[i][1]), reference[i][0]);
     do_check_eq(idnService.isACE(reference[i][1]), reference[i][2]);
  }

  // add an IDN whitelist pref
  var pbi = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefBranch);
  pbi.setBoolPref("network.IDN.whitelist.es", true);

  // After bug 722299, set network.IDN.restriction_profile to "ASCII" in
  // order not to change the behaviour of non-whitelisted TLDs
  var oldProfile = pbi.getCharPref("network.IDN.restriction_profile", "moderate");
  pbi.setCharPref("network.IDN.restriction_profile", "ASCII");

  // check convertToDisplayIDN against the whitelist
  var isASCII = {};
  do_check_eq(idnService.convertToDisplayIDN("b\u00FCcher.es", isASCII), "b\u00FCcher.es");
  do_check_eq(isASCII.value, false);
  do_check_eq(idnService.convertToDisplayIDN("xn--bcher-kva.es", isASCII), "b\u00FCcher.es");
  do_check_eq(isASCII.value, false);
  do_check_eq(idnService.convertToDisplayIDN("b\u00FCcher.uk", isASCII), "xn--bcher-kva.uk");
  do_check_eq(isASCII.value, true);
  do_check_eq(idnService.convertToDisplayIDN("xn--bcher-kva.uk", isASCII), "xn--bcher-kva.uk");
  do_check_eq(isASCII.value, true);

  // check ACE TLD's are handled by the whitelist
  pbi.setBoolPref("network.IDN.whitelist.xn--k-dha", true);
  do_check_eq(idnService.convertToDisplayIDN("test.\u00FCk", isASCII), "test.\u00FCk");
  do_check_eq(isASCII.value, false);
  do_check_eq(idnService.convertToDisplayIDN("test.xn--k-dha", isASCII), "test.\u00FCk");
  do_check_eq(isASCII.value, false);

  // reset pref to default
  pbi.setCharPref("network.IDN.restriction_profile", oldProfile);
}
