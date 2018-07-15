/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_INVALID = 0;
const PREF_BOOL    = 128;
const PREF_INT     = 64;
const PREF_STRING  = 32;

const MAX_PREF_LENGTH = 1 * 1024 * 1024;

function makeList(a)
{
  var o = {};
  for(var i=0; i<a.length; i++)
  {
    o[a[i]] = '';
  }
  return o;
}

function run_test() {

  var ps = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService);

  var pb2= Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

  var pb = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

  //**************************************************************************//
  // Nullsafety

  do_check_throws(function() {
    pb.getPrefType(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.getBoolPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.setBoolPref(null, false); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.getIntPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.setIntPref(null, 0); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.getCharPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.setCharPref(null, null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.getStringPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.setStringPref(null, null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.clearUserPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.prefHasUserValue(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.lockPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.prefIsLocked(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.unlockPref(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.deleteBranch(null); },  Cr.NS_ERROR_INVALID_ARG);
  do_check_throws(function() {
    pb.getChildList(null); },  Cr.NS_ERROR_INVALID_ARG);

  //**************************************************************************//
  // Nonexisting user preferences

  Assert.equal(pb.prefHasUserValue("UserPref.nonexistent.hasUserValue"), false);
  pb.clearUserPref("UserPref.nonexistent.clearUserPref"); // shouldn't throw
  Assert.equal(pb.getPrefType("UserPref.nonexistent.getPrefType"), PREF_INVALID);
  Assert.equal(pb.root, "");

  // bool...
  do_check_throws(function() {
    pb.getBoolPref("UserPref.nonexistent.getBoolPref");}, Cr.NS_ERROR_UNEXPECTED);
  pb.setBoolPref("UserPref.nonexistent.setBoolPref", false);
  Assert.equal(pb.getBoolPref("UserPref.nonexistent.setBoolPref"), false);

  // int...
  do_check_throws(function() {
    pb.getIntPref("UserPref.nonexistent.getIntPref");}, Cr.NS_ERROR_UNEXPECTED);
  pb.setIntPref("UserPref.nonexistent.setIntPref", 5);
  Assert.equal(pb.getIntPref("UserPref.nonexistent.setIntPref"), 5);

  // char
  do_check_throws(function() {
    pb.getCharPref("UserPref.nonexistent.getCharPref");}, Cr.NS_ERROR_UNEXPECTED);
  pb.setCharPref("UserPref.nonexistent.setCharPref", "_test");
  Assert.equal(pb.getCharPref("UserPref.nonexistent.setCharPref"), "_test");

  //**************************************************************************//
  // Existing user Prefs and data integrity test (round-trip match)

  pb.setBoolPref("UserPref.existing.bool", true);
  pb.setIntPref("UserPref.existing.int", 23);
  pb.setCharPref("UserPref.existing.char", "hey");

  // getPref should return the pref value
  Assert.equal(pb.getBoolPref("UserPref.existing.bool"), true);
  Assert.equal(pb.getIntPref("UserPref.existing.int"), 23);
  Assert.equal(pb.getCharPref("UserPref.existing.char"), "hey");

  // setPref should not complain and should change the value of the pref
  pb.setBoolPref("UserPref.existing.bool", false);
  Assert.equal(pb.getBoolPref("UserPref.existing.bool"), false);
  pb.setIntPref("UserPref.existing.int", 24);
  Assert.equal(pb.getIntPref("UserPref.existing.int"), 24);
  pb.setCharPref("UserPref.existing.char", "hej då!");
  Assert.equal(pb.getCharPref("UserPref.existing.char"), "hej då!");

  // prefHasUserValue should return true now
  Assert.ok(pb.prefHasUserValue("UserPref.existing.bool"));
  Assert.ok(pb.prefHasUserValue("UserPref.existing.int"));
  Assert.ok(pb.prefHasUserValue("UserPref.existing.char"));

  // clearUserPref should remove the pref
  pb.clearUserPref("UserPref.existing.bool");
  Assert.ok(!pb.prefHasUserValue("UserPref.existing.bool"));
  pb.clearUserPref("UserPref.existing.int");
  Assert.ok(!pb.prefHasUserValue("UserPref.existing.int"));
  pb.clearUserPref("UserPref.existing.char");
  Assert.ok(!pb.prefHasUserValue("UserPref.existing.char"));

  //**************************************************************************//
  // Large value test

  let largeStr = new Array(MAX_PREF_LENGTH + 1).join('x');
  pb.setCharPref("UserPref.large.char", largeStr);
  largeStr += 'x';
  do_check_throws(function() {
    pb.setCharPref("UserPref.large.char", largeStr); }, Cr.NS_ERROR_ILLEGAL_VALUE);

  //**************************************************************************//
  // getPrefType test

  // bool...
  pb.setBoolPref("UserPref.getPrefType.bool", true);
  Assert.equal(pb.getPrefType("UserPref.getPrefType.bool"), PREF_BOOL);

  // int...
  pb.setIntPref("UserPref.getPrefType.int", -234);
  Assert.equal(pb.getPrefType("UserPref.getPrefType.int"), PREF_INT);

  // char...
  pb.setCharPref("UserPref.getPrefType.char", "testing1..2");
  Assert.equal(pb.getPrefType("UserPref.getPrefType.char"), PREF_STRING);

  //**************************************************************************//
  // getBranch tests
  
  Assert.equal(ps.root, "");

  // bool ...
  pb.setBoolPref("UserPref.root.boolPref", true);
  let pb_1 = ps.getBranch("UserPref.root.");
  Assert.equal(pb_1.getBoolPref("boolPref"), true);
  let pb_2 = ps.getBranch("UserPref.root.boolPref");
  Assert.equal(pb_2.getBoolPref(""), true);
  pb_2.setBoolPref(".anotherPref", false);
  let pb_3 = ps.getBranch("UserPref.root.boolPre");
  Assert.equal(pb_3.getBoolPref("f.anotherPref"), false);

  // int ...
  pb.setIntPref("UserPref.root.intPref", 23);
  pb_1 = ps.getBranch("UserPref.root.");
  Assert.equal(pb_1.getIntPref("intPref"), 23);
  pb_2 = ps.getBranch("UserPref.root.intPref");
  Assert.equal(pb_2.getIntPref(""), 23);
  pb_2.setIntPref(".anotherPref", 69);
  pb_3 = ps.getBranch("UserPref.root.intPre");
  Assert.equal(pb_3.getIntPref("f.anotherPref"), 69);

  // char...
  pb.setCharPref("UserPref.root.charPref", "_char");
  pb_1 = ps.getBranch("UserPref.root.");
  Assert.equal(pb_1.getCharPref("charPref"), "_char");
  pb_2 = ps.getBranch("UserPref.root.charPref");
  Assert.equal(pb_2.getCharPref(""), "_char");
  pb_2.setCharPref(".anotherPref", "_another");
  pb_3 = ps.getBranch("UserPref.root.charPre");
  Assert.equal(pb_3.getCharPref("f.anotherPref"), "_another");

  //**************************************************************************//
  // getChildlist tests

  // get an already set prefBranch
  pb1 = ps.getBranch("UserPref.root.");
  let prefList = pb1.getChildList("");
  Assert.equal(prefList.length, 6);

  // check for specific prefs in the array : the order is not important
  Assert.ok("boolPref" in makeList(prefList));
  Assert.ok("intPref" in makeList(prefList));
  Assert.ok("charPref" in makeList(prefList));
  Assert.ok("boolPref.anotherPref" in makeList(prefList));
  Assert.ok("intPref.anotherPref" in makeList(prefList));
  Assert.ok("charPref.anotherPref" in makeList(prefList));

  //**************************************************************************//
  // Default branch tests

  // bool...
  pb1 = ps.getDefaultBranch("");
  pb1.setBoolPref("DefaultPref.bool", true);
  Assert.equal(pb1.getBoolPref("DefaultPref.bool"), true);  
  Assert.ok(!pb1.prefHasUserValue("DefaultPref.bool"));
  ps.setBoolPref("DefaultPref.bool", false);
  Assert.ok(pb1.prefHasUserValue("DefaultPref.bool"));
  Assert.equal(ps.getBoolPref("DefaultPref.bool"), false); 

  // int...
  pb1 = ps.getDefaultBranch("");
  pb1.setIntPref("DefaultPref.int", 100);
  Assert.equal(pb1.getIntPref("DefaultPref.int"), 100);  
  Assert.ok(!pb1.prefHasUserValue("DefaultPref.int"));
  ps.setIntPref("DefaultPref.int", 50);
  Assert.ok(pb1.prefHasUserValue("DefaultPref.int"));
  Assert.equal(ps.getIntPref("DefaultPref.int"), 50); 

  // char...
  pb1 = ps.getDefaultBranch("");
  pb1.setCharPref("DefaultPref.char", "_default");
  Assert.equal(pb1.getCharPref("DefaultPref.char"), "_default");  
  Assert.ok(!pb1.prefHasUserValue("DefaultPref.char"));
  ps.setCharPref("DefaultPref.char", "_user");
  Assert.ok(pb1.prefHasUserValue("DefaultPref.char"));
  Assert.equal(ps.getCharPref("DefaultPref.char"), "_user"); 

  //**************************************************************************//
  // pref Locking/Unlocking tests

  // locking and unlocking a nonexistent pref should throw
  do_check_throws(function() {
    ps.lockPref("DefaultPref.nonexistent");}, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    ps.unlockPref("DefaultPref.nonexistent");}, Cr.NS_ERROR_ILLEGAL_VALUE);

  // getting a locked pref branch should return the "default" value
  Assert.ok(!ps.prefIsLocked("DefaultPref.char"));
  ps.lockPref("DefaultPref.char");
  Assert.equal(ps.getCharPref("DefaultPref.char"), "_default"); 
  Assert.ok(ps.prefIsLocked("DefaultPref.char"));

  // getting an unlocked pref branch should return the "user" value 
  ps.unlockPref("DefaultPref.char");
  Assert.equal(ps.getCharPref("DefaultPref.char"), "_user"); 
  Assert.ok(!ps.prefIsLocked("DefaultPref.char"));

  // setting the "default" value to a user pref branch should
  // make prefHasUserValue return false (documented behavior)
  ps.setCharPref("DefaultPref.char", "_default");
  Assert.ok(!pb1.prefHasUserValue("DefaultPref.char"));

  // unlocking and locking multiple times shouldn't throw
  ps.unlockPref("DefaultPref.char");
  ps.lockPref("DefaultPref.char");
  ps.lockPref("DefaultPref.char");

  //**************************************************************************//
  // resetBranch test

  // NOT IMPLEMENTED YET in module/libpref. So we're not testing !
  // uncomment the following if resetBranch ever gets implemented.
  /*ps.resetBranch("DefaultPref");
  do_check_eq(ps.getBoolPref("DefaultPref.bool"), true);
  do_check_eq(ps.getIntPref("DefaultPref.int"), 100);
  do_check_eq(ps.getCharPref("DefaultPref.char"), "_default");*/

  //**************************************************************************//
  // deleteBranch tests

  // TODO : Really, this should throw!, by documentation.
  // do_check_throws(function() {
  // ps.deleteBranch("UserPref.nonexistent.deleteBranch");}, Cr.NS_ERROR_UNEXPECTED);

  ps.deleteBranch("DefaultPref");
  pb = ps.getBranch("DefaultPref");
  pb1 = ps.getDefaultBranch("DefaultPref");

  // getting prefs on deleted user branches should throw
  do_check_throws(function() {
    pb.getBoolPref("DefaultPref.bool");}, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    pb.getIntPref("DefaultPref.int");}, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    pb.getCharPref("DefaultPref.char");}, Cr.NS_ERROR_UNEXPECTED);

  // getting prefs on deleted default branches should throw
  do_check_throws(function() {
    pb1.getBoolPref("DefaultPref.bool");}, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    pb1.getIntPref("DefaultPref.int");}, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    pb1.getCharPref("DefaultPref.char");}, Cr.NS_ERROR_UNEXPECTED);

  //**************************************************************************//
  // savePrefFile & readPrefFile tests

  // set some prefs
  ps.setBoolPref("ReadPref.bool", true);
  ps.setIntPref("ReadPref.int", 230);
  ps.setCharPref("ReadPref.char", "hello");

  // save those prefs in a file
  let savePrefFile = do_get_cwd();
  savePrefFile.append("data");
  savePrefFile.append("savePref.js");

  if (savePrefFile.exists())
    savePrefFile.remove(false);
  savePrefFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  ps.savePrefFile(savePrefFile);
  ps.resetPrefs();

  // load a preexisting pref file
  let prefFile = do_get_file("data/testPref.js");
  ps.readUserPrefsFromFile(prefFile);

  // former prefs should have been replaced/lost
  do_check_throws(function() {
    pb.getBoolPref("ReadPref.bool");}, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    pb.getIntPref("ReadPref.int");}, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    pb.getCharPref("ReadPref.char");}, Cr.NS_ERROR_UNEXPECTED);

  // loaded prefs should read ok.
  pb = ps.getBranch("testPref.");
  Assert.equal(pb.getBoolPref("bool1"), true);
  Assert.equal(pb.getBoolPref("bool2"), false);
  Assert.equal(pb.getIntPref("int1"), 23);
  Assert.equal(pb.getIntPref("int2"), -1236);
  Assert.equal(pb.getCharPref("char1"), "_testPref");
  Assert.equal(pb.getCharPref("char2"), "älskar");

  // loading our former savePrefFile should allow us to read former prefs

  // Hack alert: on Windows nsLocalFile caches the size of savePrefFile from
  // the .create() call above as 0. We call .exists() to reset the cache.
  savePrefFile.exists();

  ps.readUserPrefsFromFile(savePrefFile);
  // cleanup the file now we don't need it
  savePrefFile.remove(false);
  Assert.equal(ps.getBoolPref("ReadPref.bool"), true);
  Assert.equal(ps.getIntPref("ReadPref.int"), 230);
  Assert.equal(ps.getCharPref("ReadPref.char"), "hello");

  // ... and still be able to access "prior-to-readUserPrefs" preferences
  Assert.equal(pb.getBoolPref("bool1"), true);
  Assert.equal(pb.getBoolPref("bool2"), false);
  Assert.equal(pb.getIntPref("int1"), 23);

  //**************************************************************************//
  // preference Observers

  class PrefObserver {
    /**
     * Creates and registers a pref observer.
     *
     * @param prefBranch The preference branch instance to observe.
     * @param expectedName The pref name we expect to receive.
     * @param expectedValue The int pref value we expect to receive.
     */
    constructor(prefBranch, expectedName, expectedValue) {
      this.pb = prefBranch;
      this.name = expectedName;
      this.value = expectedValue;

      prefBranch.addObserver(expectedName, this);
    }

    QueryInterface(aIID) {
      if (aIID.equals(Ci.nsIObserver) ||
          aIID.equals(Ci.nsISupports))
         return this;
      throw Cr.NS_NOINTERFACE;
    }

    observe(aSubject, aTopic, aState) {
      Assert.equal(aTopic, "nsPref:changed");
      Assert.equal(aState, this.name);
      Assert.equal(this.pb.getIntPref(aState), this.value);
      pb.removeObserver(aState, this);

      // notification received, we may go on...
      do_test_finished();
    }
  };

  // Indicate that we'll have 3 more async tests pending so that they all
  // actually get a chance to run.
  do_test_pending();
  do_test_pending();
  do_test_pending();

  let observer = new PrefObserver(pb2, "ReadPref.int", 76);
  ps.setIntPref("ReadPref.int", 76);

  // removed observer should not fire
  pb2.removeObserver("ReadPref.int", observer);
  ps.setIntPref("ReadPref.int", 32);

  // let's test observers once more with a non-root prefbranch
  pb = pb2.getBranch("ReadPref.");
  observer = new PrefObserver(pb, "int", 76);
  ps.setIntPref("ReadPref.int", 76);

  // Let's try that again with different pref.
  observer = new PrefObserver(pb, "another_int", 76);
  ps.setIntPref("ReadPref.another_int", 76);
}
