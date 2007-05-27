// Copied from components/places/tests/unit/head_bookmarks.js

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

// If there's no location registered for the profile direcotry, register one now
var dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                    .getService(Ci.nsIProperties);

try {
  var profileDir = dirSvc.get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);
} catch (e) { }

if (!profileDir) {
  // Register our own provider for the profile directory.
  // It will simply return the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == NS_APP_USER_PROFILE_50_DIR) {
        return dirSvc.get("CurProcD", Ci.nsIFile);
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };

  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}


// Copy key3.db into the proper place.
var keydb = do_get_file("toolkit/components/passwordmgr/test/unit/key3.db");
profileDir = dirSvc.get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);

// Remove the file if it already exists. key3.db can be create if it
// doesn't exist, so one might accidently end up creating it and it will
// have the wrong key.
try {
    var oldfile = Cc["@mozilla.org/file/local;1"]
                        .createInstance(Ci.nsILocalFile);
    oldfile.initWithPath(profileDir.path + "/key3.db");
    if (oldfile.exists())
        oldfile.remove(false);
} catch(e) { }

// Copy the key3.db for testing into place.
keydb.copyTo(profileDir, "key3.db");
