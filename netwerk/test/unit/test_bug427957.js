/**
 * Test for Bidi restrictions on IDNs from RFC 3454
 */

var Cc = Components.classes;
var Ci = Components.interfaces;
var idnService;

function expected_pass(inputIDN)
{
  var isASCII = {};
  var displayIDN = idnService.convertToDisplayIDN(inputIDN, isASCII);
  do_check_eq(displayIDN, inputIDN);
}

function expected_fail(inputIDN)
{
  var isASCII = {};
  var displayIDN = "";

  try {
    displayIDN = idnService.convertToDisplayIDN(inputIDN, isASCII);
  }
  catch(e) {}

  do_check_neq(displayIDN, inputIDN);
}

function run_test() {
   // add an IDN whitelist pref
  var pbi = Cc["@mozilla.org/preferences-service;1"]
    .getService(Ci.nsIPrefBranch);
  pbi.setBoolPref("network.IDN.whitelist.com", true);
 
  idnService = Cc["@mozilla.org/network/idn-service;1"]
    .getService(Ci.nsIIDNService);
  /*
   * In any profile that specifies bidirectional character handling, all
   * three of the following requirements MUST be met:
   *
   * 1) The characters in section 5.8 MUST be prohibited.
   */

  // 0340; COMBINING GRAVE TONE MARK
  expected_fail("foo\u0340bar.com");
  // 0341; COMBINING ACUTE TONE MARK
  expected_fail("foo\u0341bar.com");
  // 200E; LEFT-TO-RIGHT MARK
  expected_fail("foo\200ebar.com");
  // 200F; RIGHT-TO-LEFT MARK
  //  Note: this is an RTL IDN so that it doesn't fail test 2) below
  expected_fail("\u200f\u0645\u062B\u0627\u0644.\u0622\u0632\u0645\u0627\u06CC\u0634\u06CC");
  // 202A; LEFT-TO-RIGHT EMBEDDING
  expected_fail("foo\u202abar.com");
  // 202B; RIGHT-TO-LEFT EMBEDDING
  expected_fail("foo\u202bbar.com");
  // 202C; POP DIRECTIONAL FORMATTING
  expected_fail("foo\u202cbar.com");
  // 202D; LEFT-TO-RIGHT OVERRIDE
  expected_fail("foo\u202dbar.com");
  // 202E; RIGHT-TO-LEFT OVERRIDE
  expected_fail("foo\u202ebar.com");
  // 206A; INHIBIT SYMMETRIC SWAPPING
  expected_fail("foo\u206abar.com");
  // 206B; ACTIVATE SYMMETRIC SWAPPING
  expected_fail("foo\u206bbar.com");
  // 206C; INHIBIT ARABIC FORM SHAPING
  expected_fail("foo\u206cbar.com");
  // 206D; ACTIVATE ARABIC FORM SHAPING
  expected_fail("foo\u206dbar.com");
  // 206E; NATIONAL DIGIT SHAPES
  expected_fail("foo\u206ebar.com");
  // 206F; NOMINAL DIGIT SHAPES
  expected_fail("foo\u206fbar.com");

  /*
   * 2) If a string contains any RandALCat character, the string MUST NOT
   *    contain any LCat character.
   */   

  // www.מיץpetel.com is invalid
  expected_fail("www.\u05DE\u05D9\u05E5petel.com");
  // But www.מיץפטל.com is fine because the ltr and rtl characters are in
  // different labels
  expected_pass("www.\u05DE\u05D9\u05E5\u05E4\u05D8\u05DC.com");

  /*
   * 3) If a string contains any RandALCat character, a RandALCat
   *    character MUST be the first character of the string, and a
   *    RandALCat character MUST be the last character of the string.
   */

  // www.1מיץ.com is invalid
  expected_fail("www.1\u05DE\u05D9\u05E5.com");
  // www.!מיץ.com is invalid
  expected_fail("www.!\u05DE\u05D9\u05E5.com");
  // www.מיץ!.com is invalid
  expected_fail("www.\u05DE\u05D9\u05E5!.com");

  // XXX TODO: add a test for an RTL label ending with a digit. This was
  //           invalid in IDNA2003 but became valid in IDNA2008

  // But www.מיץ1פטל.com is fine
  expected_pass("www.\u05DE\u05D9\u05E51\u05E4\u05D8\u05DC.com");
}
  
