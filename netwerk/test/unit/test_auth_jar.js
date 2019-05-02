function createURI(s) {
  let service = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);
  return service.newURI(s);
}
 
function run_test() {
  // Set up a profile.
  do_get_profile();

  var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
  const kURI1 = "http://example.com";
  var app = secMan.createCodebasePrincipal(createURI(kURI1), {});
  var appbrowser = secMan.createCodebasePrincipal(createURI(kURI1), {inIsolatedMozBrowser: true});

  var am = Cc["@mozilla.org/network/http-auth-manager;1"].
           getService(Ci.nsIHttpAuthManager);
  am.setAuthIdentity("http", "a.example.com", -1, "basic", "realm", "", "example.com", "user", "pass", false, app);
  am.setAuthIdentity("http", "a.example.com", -1, "basic", "realm", "", "example.com", "user3", "pass3", false, appbrowser);

  Services.clearData.deleteDataFromOriginAttributesPattern({ inIsolatedMozBrowser:true });
  
  var domain = {value: ""}, user = {value: ""}, pass = {value: ""};
  try {
    am.getAuthIdentity("http", "a.example.com", -1, "basic", "realm", "", domain, user, pass, false, appbrowser);
    Assert.equal(false, true); // no identity should be present
  } catch (x) {
    Assert.equal(domain.value, "");
    Assert.equal(user.value, "");
    Assert.equal(pass.value, "");
  }

  am.getAuthIdentity("http", "a.example.com", -1, "basic", "realm", "", domain, user, pass, false, app);
  Assert.equal(domain.value, "example.com");
  Assert.equal(user.value, "user");
  Assert.equal(pass.value, "pass");
}
