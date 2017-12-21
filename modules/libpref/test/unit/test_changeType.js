/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Tests for changing the type of a preference (bug 985998) */

const PREF_INVALID = 0;
const PREF_BOOL    = 128;
const PREF_INT     = 64;
const PREF_STRING  = 32;

function run_test() {

  var ps = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService);

  let defaultBranch = ps.getDefaultBranch("");
  let userBranch = ps.getBranch("");

  // Prefs that only have a default value -- we can't change their type.
  defaultBranch.setBoolPref("TypeTest.default.bool", true);
  defaultBranch.setIntPref("TypeTest.default.int", 23);
  defaultBranch.setCharPref("TypeTest.default.char", "hey");

  Assert.equal(userBranch.getBoolPref("TypeTest.default.bool"), true);
  Assert.equal(userBranch.getIntPref("TypeTest.default.int"), 23);
  Assert.equal(userBranch.getCharPref("TypeTest.default.char"), "hey");

  // Prefs that only have a user value -- we can change their type, but only
  // when we set the user value.
  userBranch.setBoolPref("TypeTest.user.bool", false);
  userBranch.setIntPref("TypeTest.user.int", 24);
  userBranch.setCharPref("TypeTest.user.char", "hi");

  Assert.equal(userBranch.getBoolPref("TypeTest.user.bool"), false);
  Assert.equal(userBranch.getIntPref("TypeTest.user.int"), 24);
  Assert.equal(userBranch.getCharPref("TypeTest.user.char"), "hi");

  // Prefs that have both a default and a user value -- we can't change their
  // type.
  defaultBranch.setBoolPref("TypeTest.both.bool", true);
     userBranch.setBoolPref("TypeTest.both.bool", false);
  defaultBranch.setIntPref("TypeTest.both.int", 25);
     userBranch.setIntPref("TypeTest.both.int", 26);
  defaultBranch.setCharPref("TypeTest.both.char", "yo");
     userBranch.setCharPref("TypeTest.both.char", "ya");

  Assert.equal(userBranch.getBoolPref("TypeTest.both.bool"), false);
  Assert.equal(userBranch.getIntPref("TypeTest.both.int"), 26);
  Assert.equal(userBranch.getCharPref("TypeTest.both.char"), "ya");

  // We only have a default value, and we try to set a default value of a
  // different type --> fails.
  do_check_throws(function() {
    defaultBranch.setCharPref("TypeTest.default.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setIntPref("TypeTest.default.bool", 5); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setCharPref("TypeTest.default.int", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setBoolPref("TypeTest.default.int", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setBoolPref("TypeTest.default.char", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setIntPref("TypeTest.default.char", 6); }, Cr.NS_ERROR_UNEXPECTED);

  // We only have a default value, and we try to set a user value of a
  // different type --> fails.
  do_check_throws(function() {
    userBranch.setCharPref("TypeTest.default.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setIntPref("TypeTest.default.bool", 5); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setCharPref("TypeTest.default.int", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setBoolPref("TypeTest.default.int", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setBoolPref("TypeTest.default.char", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setIntPref("TypeTest.default.char", 6); }, Cr.NS_ERROR_UNEXPECTED);

  // We only have a user value, and we try to set a default value of a
  // different type --> fails.
  do_check_throws(function() {
    defaultBranch.setCharPref("TypeTest.user.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setIntPref("TypeTest.user.bool", 5); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setCharPref("TypeTest.user.int", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setBoolPref("TypeTest.user.int", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setBoolPref("TypeTest.user.char", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setIntPref("TypeTest.user.char", 6); }, Cr.NS_ERROR_UNEXPECTED);

  // We only have a user value, and we try to set a user value of a
  // different type --> SUCCEEDS.
  userBranch.setCharPref("TypeTest.user.bool", "boo");
  Assert.equal(userBranch.getCharPref("TypeTest.user.bool"), "boo");
  userBranch.setIntPref("TypeTest.user.bool", 5);
  Assert.equal(userBranch.getIntPref("TypeTest.user.bool"), 5);
  userBranch.setCharPref("TypeTest.user.int", "boo");
  Assert.equal(userBranch.getCharPref("TypeTest.user.int"), "boo");
  userBranch.setBoolPref("TypeTest.user.int", true);
  Assert.equal(userBranch.getBoolPref("TypeTest.user.int"), true);
  userBranch.setBoolPref("TypeTest.user.char", true);
  Assert.equal(userBranch.getBoolPref("TypeTest.user.char"), true);
  userBranch.setIntPref("TypeTest.user.char", 6);
  Assert.equal(userBranch.getIntPref("TypeTest.user.char"), 6);

  // We have both a default value and user value, and we try to set a default
  // value of a different type --> fails.
  do_check_throws(function() {
    defaultBranch.setCharPref("TypeTest.both.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setIntPref("TypeTest.both.bool", 5); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setCharPref("TypeTest.both.int", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setBoolPref("TypeTest.both.int", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setBoolPref("TypeTest.both.char", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    defaultBranch.setIntPref("TypeTest.both.char", 6); }, Cr.NS_ERROR_UNEXPECTED);

  // We have both a default value and user value, and we try to set a user
  // value of a different type --> fails.
  do_check_throws(function() {
    userBranch.setCharPref("TypeTest.both.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setIntPref("TypeTest.both.bool", 5); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setCharPref("TypeTest.both.int", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setBoolPref("TypeTest.both.int", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setBoolPref("TypeTest.both.char", true); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    userBranch.setIntPref("TypeTest.both.char", 6); }, Cr.NS_ERROR_UNEXPECTED);
}
