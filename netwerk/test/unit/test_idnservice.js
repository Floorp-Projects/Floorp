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
}
