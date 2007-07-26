function dumpn(s) {
  dump(s + "\n");
}

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const NS_APP_USER_PROFILE_LOCAL_50_DIR = "ProfLD";
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

// If there's no location registered for the profile direcotry, register one now.
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var profileDir = null;
try {
  profileDir = dirSvc.get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);
} catch (e) {}

if (!profileDir) {
  // Register our own provider for the profile directory.
  // It will simply return the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == NS_APP_USER_PROFILE_50_DIR ||
          prop == NS_APP_USER_PROFILE_LOCAL_50_DIR) {
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

var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

function cleanUp() {
  try {
    // Delete a previously created sqlite file
    var file = dirSvc.get('ProfLD', Ci.nsIFile);
    file.append("urlclassifier3.sqlite");
    if (file.exists())
      file.remove(false);
  } catch (e) {}
}
cleanUp();
