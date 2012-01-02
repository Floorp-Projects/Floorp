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
  "<!DOCTYPE html><math><mtext><p><i></p>a" : true, // Bug 711049
  "<!DOCTYPE html><table><tr><td><math><mtext><p><i></p>a" : true, // Bug 711049
  "<!DOCTYPE html><table><tr><td><math><mtext>\u0000a" : true, // Bug 711049
  "<!DOCTYPE html><math><mi>a\u0000b" : true, // Bug 711049
  "<!DOCTYPE html><math><mo>a\u0000b" : true, // Bug 711049
  "<!DOCTYPE html><math><mn>a\u0000b" : true, // Bug 711049
  "<!DOCTYPE html><math><ms>a\u0000b" : true, // Bug 711049
  "<!DOCTYPE html><math><mtext>a\u0000b" : true, // Bug 711049
}
