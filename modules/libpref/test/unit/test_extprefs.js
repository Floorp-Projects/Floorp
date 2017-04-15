/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

// The profile directory is already set up in the head_ files.

function arrayenumerator(a)
{
  return {
    i_: 0,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),
    hasMoreElements: function ae_hasMoreElements() {
      return this.i_ < a.length;
    },
    getNext: function ae_getNext() {
      return a[this.i_++];
    }
  };
}

function run_test() {
  var ps = Cc["@mozilla.org/preferences-service;1"].
    getService(Ci.nsIPrefService).QueryInterface(Ci.nsIPrefBranch);

  var extprefs = [do_get_file("extdata")];
  
  var extProvider = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider,
                                           Ci.nsIDirectoryServiceProvider2]),
    getFile: function ep_getFile() {
      throw Cr.NS_ERROR_FAILURE;
    },
    
    getFiles: function ep_getFiles(key) {
      if (key != "ExtPrefDL")
        throw Cr.NS_ERROR_FAILURE;

      return arrayenumerator(extprefs);
    }
  };
  
  let prefFile = do_get_file("data/testPref.js");

  do_check_throws(function() {
    ps.getBoolPref("testExtPref.bool");
  }, Cr.NS_ERROR_UNEXPECTED);
  do_check_throws(function() {
    ps.getBoolPref("testPref.bool1");
  }, Cr.NS_ERROR_UNEXPECTED);
  
  ps.readUserPrefs(prefFile);

  do_check_true(ps.getBoolPref("testPref.bool1"));
  ps.setBoolPref("testPref.bool1", false);
  do_check_false(ps.getBoolPref("testPref.bool1"));
  
  dirSvc.registerProvider(extProvider);
  Services.obs.notifyObservers(null, "load-extension-defaults");

  // The extension default should be available.
  do_check_true(ps.getBoolPref("testExtPref.bool"));

  // The extension default should not override existing user prefs
  do_check_false(ps.getBoolPref("testPref.bool2"));

  // The extension default should not modify existing set values
  do_check_false(ps.getBoolPref("testPref.bool1"));
}
