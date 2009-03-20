function test() {
  // simple test to confirm we have chrome privileges
  let hasPrivileges = true;

  // this will throw an exception if we are not running with privileges
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);
  }
  catch (e) {
    hasPrivileges = false;
  }

  // if we get here, we must have chrome privileges
  ok(hasPrivileges, "running with chrome privileges");
}
