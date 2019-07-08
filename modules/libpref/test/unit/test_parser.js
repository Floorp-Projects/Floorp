/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  const ps = Services.prefs;

  ps.resetPrefs();
  ps.readDefaultPrefsFromFile(do_get_file("data/testParser.js"));

  Assert.equal(ps.getBoolPref("comment1"), true);
  Assert.equal(ps.getBoolPref("comment2"), true);
  Assert.equal(ps.getBoolPref("spaced-out"), true);

  Assert.equal(ps.getBoolPref("pref"), true);
  Assert.equal(ps.getBoolPref("sticky_pref"), true);
  Assert.equal(ps.getBoolPref("user_pref"), true);
  Assert.equal(ps.getBoolPref("sticky_pref2"), true);
  Assert.equal(ps.getBoolPref("locked_pref"), true);
  Assert.equal(ps.getBoolPref("locked_sticky_pref"), true);
  Assert.equal(ps.prefIsLocked("locked_pref"), true);
  Assert.equal(ps.prefIsLocked("locked_sticky_pref"), true);

  Assert.equal(ps.getBoolPref("bool.true"), true);
  Assert.equal(ps.getBoolPref("bool.false"), false);

  Assert.equal(ps.getIntPref("int.0"), 0);
  Assert.equal(ps.getIntPref("int.1"), 1);
  Assert.equal(ps.getIntPref("int.123"), 123);
  Assert.equal(ps.getIntPref("int.+234"), 234);
  Assert.equal(ps.getIntPref("int.+  345"), 345);
  Assert.equal(ps.getIntPref("int.-0"), -0);
  Assert.equal(ps.getIntPref("int.-1"), -1);
  Assert.equal(ps.getIntPref("int.- /* hmm */\t456"), -456);
  Assert.equal(ps.getIntPref("int.-\n567"), -567);
  Assert.equal(ps.getIntPref("int.INT_MAX-1"), 2147483646);
  Assert.equal(ps.getIntPref("int.INT_MAX"), 2147483647);
  Assert.equal(ps.getIntPref("int.INT_MIN+2"), -2147483646);
  Assert.equal(ps.getIntPref("int.INT_MIN+1"), -2147483647);
  Assert.equal(ps.getIntPref("int.INT_MIN"), -2147483648);

  Assert.equal(ps.getCharPref("string.empty"), "");
  Assert.equal(ps.getCharPref("string.abc"), "abc");
  Assert.equal(
    ps.getCharPref("string.long"),
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
  );
  Assert.equal(ps.getCharPref("string.single-quotes"), '"abc"');
  Assert.equal(ps.getCharPref("string.double-quotes"), "'abc'");
  Assert.equal(
    ps.getCharPref("string.weird-chars"),
    "\x0d \x09 \x0b \x0c \x06 \x16"
  );
  Assert.equal(ps.getCharPref("string.escapes"), "\" ' \\ \n \r");

  // This one is ASCII, so we can use getCharPref() and getStringPref
  // interchangeably.
  Assert.equal(
    ps.getCharPref("string.x-escapes1"),
    "Mozilla0\x4d\x6F\x7a\x69\x6c\x6C\x610"
  );
  Assert.equal(ps.getStringPref("string.x-escapes1"), "Mozilla0Mozilla0");

  // This one has chars with value > 127, so it's not valid UTF8, so we can't
  // use getStringPref on it.
  Assert.equal(
    ps.getCharPref("string.x-escapes2"),
    "AA A_umlaut\xc4 y_umlaut\xff"
  );

  // The following strings use \uNNNN escapes, which are UTF16 code points.
  // libpref stores them internally as UTF8 byte sequences. In each case we get
  // the string in two ways:
  // - getStringPref() interprets it as UTF8, which is then converted to UTF16
  //   in JS. I.e. code points that are multiple bytes in UTF8 become a single
  //   16-bit char in JS (except for the non-BMP chars, which become a 16-bit
  //   surrogate pair).
  // - getCharPref() interprets it as Latin1, which is then converted to UTF16
  //   in JS. I.e. code points that are multiple bytes in UTF8 become multiple
  //   16-bit chars in JS.

  Assert.equal(
    ps.getStringPref("string.u-escapes1"),
    "A\u0041 A_umlaut\u00c4 y_umlaut\u00ff0"
  );
  Assert.equal(
    ps.getCharPref("string.u-escapes1"),
    "A\x41 A_umlaut\xc3\x84 y_umlaut\xc3\xbf0"
  );

  Assert.equal(
    ps.getStringPref("string.u-escapes2"),
    "S_acute\u015a y_grave\u1Ef3"
  );
  Assert.equal(
    ps.getCharPref("string.u-escapes2"),
    "S_acute\xc5\x9a y_grave\xe1\xbb\xb3"
  );

  Assert.equal(
    ps.getStringPref("string.u-surrogates"),
    "cyclone\uD83C\uDF00 grinning_face\uD83D\uDE00"
  );
  Assert.equal(
    ps.getCharPref("string.u-surrogates"),
    "cyclone\xF0\x9F\x8C\x80 grinning_face\xF0\x9F\x98\x80"
  );
}
