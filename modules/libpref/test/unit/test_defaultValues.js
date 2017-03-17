/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Tests for providing a default value to get{Bool,Char,Float,Int}Pref */

function run_test() {
  var ps = Cc["@mozilla.org/preferences-service;1"]
             .getService(Ci.nsIPrefService)
             .QueryInterface(Ci.nsIPrefBranch);

  let prefName = "test.default.values.bool";
  do_check_throws(function() { ps.getBoolPref(prefName); },
                  Cr.NS_ERROR_UNEXPECTED);
  strictEqual(ps.getBoolPref(prefName, false), false);
  strictEqual(ps.getBoolPref(prefName, true), true);
  ps.setBoolPref(prefName, true);
  strictEqual(ps.getBoolPref(prefName), true);
  strictEqual(ps.getBoolPref(prefName, false), true);
  strictEqual(ps.getBoolPref(prefName, true), true);

  prefName = "test.default.values.char";
  do_check_throws(function() { ps.getCharPref(prefName); },
                  Cr.NS_ERROR_UNEXPECTED);
  strictEqual(ps.getCharPref(prefName, ""), "");
  strictEqual(ps.getCharPref(prefName, "string"), "string");
  ps.setCharPref(prefName, "foo");
  strictEqual(ps.getCharPref(prefName), "foo");
  strictEqual(ps.getCharPref(prefName, "string"), "foo");

  prefName = "test.default.values.string";
  do_check_throws(function() { ps.getCharPref(prefName); },
                  Cr.NS_ERROR_UNEXPECTED);
  strictEqual(ps.getStringPref(prefName, ""), "");
  strictEqual(ps.getStringPref(prefName, "éèçàê€"), "éèçàê€");
  ps.setStringPref(prefName, "éèçàê€");
  strictEqual(ps.getStringPref(prefName), "éèçàê€");
  strictEqual(ps.getStringPref(prefName, "string"), "éèçàê€");
  strictEqual(ps.getStringPref(prefName),
              ps.getComplexValue(prefName, Ci.nsISupportsString).data);
  let str = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
  str.data = "ù€ÚîœïŒëøÇ“";
  ps.setComplexValue(prefName, Ci.nsISupportsString, str);
  strictEqual(ps.getStringPref(prefName), "ù€ÚîœïŒëøÇ“");

  prefName = "test.default.values.float";
  do_check_throws(function() { ps.getFloatPref(prefName); },
                  Cr.NS_ERROR_UNEXPECTED);
  strictEqual(ps.getFloatPref(prefName, 3.5), 3.5);
  strictEqual(ps.getFloatPref(prefName, 0), 0);
  ps.setCharPref(prefName, 1.75);
  strictEqual(ps.getFloatPref(prefName), 1.75);
  strictEqual(ps.getFloatPref(prefName, 3.5), 1.75);

  prefName = "test.default.values.int";
  do_check_throws(function() { ps.getIntPref(prefName); },
                  Cr.NS_ERROR_UNEXPECTED);
  strictEqual(ps.getIntPref(prefName, 3), 3);
  strictEqual(ps.getIntPref(prefName, 0), 0);
  ps.setIntPref(prefName, 42);
  strictEqual(ps.getIntPref(prefName), 42);
  strictEqual(ps.getIntPref(prefName, 3), 42);
}
