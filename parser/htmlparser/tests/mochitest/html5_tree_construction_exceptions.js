/*
 * These are the tests we don't pass. The test data comes from the .dat
 * files under html5lib_tree_construction/. Please see
 * html5lib_tree_construction/html5lib_license.txt for the license for these
 * tests.
 */
var html5Exceptions = {
  "<!doctype html><keygen><frameset>" : true, // Bug 101019
  "<select><keygen>" : true, // Bug 101019
  "<math><mi><div><object><div><span></span></div></object></div></mi><mi>" : true, // Bug 606925
  "<plaintext>\u0000filler\u0000text\u0000" : true, // Bug 612527
  "<body><svg><foreignObject>\u0000filler\u0000text" : true, // Bug 612527
  "<svg>\u0000</svg><frameset>" : true, // Bug 612527
  "<svg>\u0000 </svg><frameset>" : true, // Bug 612527
  "<option><span><option>" : true, // Bug 612528
  "<math><annotation-xml encoding=\"application/xhtml+xml\"><div>" : true, // Bug 612529
  "<math><annotation-xml encoding=\"aPPlication/xhtmL+xMl\"><div>" : true, // Bug 612529
  "<math><annotation-xml encoding=\"text/html\"><div>" : true, // Bug 612529
  "<math><annotation-xml encoding=\"Text/htmL\"><div>" : true, // Bug 612529
}
