/*
 * These are the tests we don't pass. The test data comes from the .dat
 * files under html5lib_tree_construction/. Please see
 * html5lib_tree_construction/html5lib_license.txt for the license for these
 * tests.
 */
var html5Exceptions = {
  "<!doctype html><keygen><frameset>" : true, // Bug 101019
  "<select><keygen>" : true, // Bug 101019
  "<!doctype html><p><summary>" : true, // Bug 596169
  "<!doctype html><summary><article></summary>a" : true, // Bug 596171
  "<!doctype html><p><figcaption>" : true, // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10589
  "<!doctype html><figcaption><article></figcaption>a" : true, // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10589
}
