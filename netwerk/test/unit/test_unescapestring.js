const ONLY_NONASCII = Components.interfaces.nsINetUtil.ESCAPE_URL_ONLY_NONASCII;
const SKIP_CONTROL = Components.interfaces.nsINetUtil.ESCAPE_URL_SKIP_CONTROL;


var tests = [
  ["foo", "foo", 0],
  ["foo%20bar", "foo bar", 0],
  ["foo%2zbar", "foo%2zbar", 0],
  ["foo%", "foo%", 0],
  ["%zzfoo", "%zzfoo", 0],
  ["foo%z", "foo%z", 0],
  ["foo%00bar", "foo\x00bar", 0],
  ["foo%ffbar", "foo\xffbar", 0],
  ["%00%1b%20%61%7f%80%ff", "%00%1b%20%61%7f\x80\xff", ONLY_NONASCII],
  ["%00%1b%20%61%7f%80%ff", "%00%1b a%7f\x80\xff", SKIP_CONTROL],
  ["%00%1b%20%61%7f%80%ff", "%00%1b%20%61%7f\x80\xff", ONLY_NONASCII|SKIP_CONTROL],
  // Test that we do not drop the high-bytes of a UTF-16 string.
  ["\u30a8\u30c9", "\xe3\x82\xa8\xe3\x83\x89", 0],
];

function run_test() {
  var util = Components.classes["@mozilla.org/network/util;1"]
                       .getService(Components.interfaces.nsINetUtil);

  for (var i = 0; i < tests.length; ++i) {
    dump("Test " + i + " (" + tests[i][0] + ", " + tests[i][2] + ")\n");
    do_check_eq(util.unescapeString(tests[i][0], tests[i][2]),
                                    tests[i][1]);
  }
  dump(tests.length + " tests passed\n");
}
