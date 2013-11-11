/*
 * These are the tests we don't pass. The test data comes from the .dat
 * files under html5lib_tree_construction/. Please see
 * html5lib_tree_construction/html5lib_license.txt for the license for these
 * tests.
 */
var html5Exceptions = {
  "<!doctype html><keygen><frameset>" : true, // Bug 101019
  "<select><keygen>" : true, // Bug 101019
  "<!DOCTYPE html><body><keygen>A" : true, // Bug 101019
}
