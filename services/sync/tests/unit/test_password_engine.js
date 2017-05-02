Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const LoginInfo = Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");

const PropertyBag = Components.Constructor(
  "@mozilla.org/hash-property-bag;1", Ci.nsIWritablePropertyBag);

add_task(async function test_password_engine() {
  _("Basic password sync test");

  let engine = Service.engineManager.get("passwords");
  let store = engine._store;

  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);
  let collection = server.user("foo").collection("passwords");

  enableValidationPrefs();

  _("Add new login to upload during first sync");
  let newLogin;
  {
    let login = new LoginInfo("https://example.com", "", null, "username",
      "password", "", "");
    Services.logins.addLogin(login);

    let logins = Services.logins.findLogins({}, "https://example.com", "", "");
    equal(logins.length, 1, "Should find new login in login manager");
    newLogin = logins[0].QueryInterface(Ci.nsILoginMetaInfo);

    // Insert a server record that's older, so that we prefer the local one.
    let rec = new LoginRec("passwords", newLogin.guid);
    rec.formSubmitURL = newLogin.formSubmitURL;
    rec.httpRealm = newLogin.httpRealm;
    rec.hostname = newLogin.hostname;
    rec.username = newLogin.username;
    rec.password = "sekrit";
    let remotePasswordChangeTime = Date.now() - 1 * 60 * 60 * 24 * 1000;
    rec.timeCreated = remotePasswordChangeTime;
    rec.timePasswordChanged = remotePasswordChangeTime;
    collection.insert(newLogin.guid, encryptPayload(rec.cleartext),
      remotePasswordChangeTime / 1000);
  }

  _("Add login with older password change time to replace during first sync");
  let oldLogin;
  {
    let login = new LoginInfo("https://mozilla.com", "", null, "us3r",
      "0ldpa55", "", "");
    Services.logins.addLogin(login);

    let props = new PropertyBag();
    let localPasswordChangeTime = Date.now() - 1 * 60 * 60 * 24 * 1000;
    props.setProperty("timePasswordChanged", localPasswordChangeTime);
    Services.logins.modifyLogin(login, props);

    let logins = Services.logins.findLogins({}, "https://mozilla.com", "", "");
    equal(logins.length, 1, "Should find old login in login manager");
    oldLogin = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
    equal(oldLogin.timePasswordChanged, localPasswordChangeTime);

    let rec = new LoginRec("passwords", oldLogin.guid);
    rec.hostname = oldLogin.hostname;
    rec.formSubmitURL = oldLogin.formSubmitURL;
    rec.httpRealm = oldLogin.httpRealm;
    rec.username = oldLogin.username;
    // Change the password and bump the password change time to ensure we prefer
    // the remote one during reconciliation.
    rec.password = "n3wpa55";
    rec.usernameField = oldLogin.usernameField;
    rec.passwordField = oldLogin.usernameField;
    rec.timeCreated = oldLogin.timeCreated;
    rec.timePasswordChanged = Date.now();
    collection.insert(oldLogin.guid, encryptPayload(rec.cleartext));
  }

  Svc.Obs.notify("weave:engine:start-tracking");

  try {
    await sync_engine_and_validate_telem(engine, false);

    let newRec = JSON.parse(JSON.parse(
      collection.payload(newLogin.guid)).ciphertext);
    equal(newRec.password, "password",
      "Should update remote password for newer login");

    let logins = Services.logins.findLogins({}, "https://mozilla.com", "", "");
    equal(logins[0].password, "n3wpa55",
      "Should update local password for older login");
  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await promiseStopServer(server);
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
});
