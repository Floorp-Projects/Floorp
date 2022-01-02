/*
 * These are the tests we don't pass. The test data comes from the .dat
 * files under html5lib_tree_construction/. Please see
 * html5lib_tree_construction/html5lib_license.txt for the license for these
 * tests.
 */
var html5Exceptions = {
  "<select><keygen>": true, // Bug 101019
  "<p><table></p>": true, // parser_web_testrunner.js uses srcdoc which forces quirks mode
  "<p><table></table>": true, // parser_web_testrunner.js uses srcdoc which forces quirks mode
};
