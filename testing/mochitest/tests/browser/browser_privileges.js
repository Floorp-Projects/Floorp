function test() {
  // simple test to confirm we have chrome privileges
  let hasPrivileges = true;

  // this will throw an exception if we are not running with privileges
  try {
    // eslint-disable-next-line no-unused-vars, mozilla/use-services
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
  } catch (e) {
    hasPrivileges = false;
  }

  // if we get here, we must have chrome privileges
  ok(hasPrivileges, "running with chrome privileges");
}
