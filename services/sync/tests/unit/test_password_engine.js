Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const LoginInfo = Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");

const PropertyBag = Components.Constructor(
  "@mozilla.org/hash-property-bag;1", Ci.nsIWritablePropertyBag);

function run_test() {
  Service.engineManager.unregister("addons"); // To silence errors.
  run_next_test();
}

async function cleanup(engine, server) {
  Svc.Obs.notify("weave:engine:stop-tracking");
  await engine.wipeClient();
  Svc.Prefs.resetBranch("");
  Service.recordManager.clearCache();
  await promiseStopServer(server);
}

add_task(async function test_ignored_fields() {
  _("Only changes to syncable fields should be tracked");

  let engine = Service.engineManager.get("passwords");

  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  enableValidationPrefs();

  let login = Services.logins.addLogin(new LoginInfo("https://example.com", "",
    null, "username", "password", "", ""));
  login.QueryInterface(Ci.nsILoginMetaInfo); // For `guid`.

  Svc.Obs.notify("weave:engine:start-tracking");

  try {
    let nonSyncableProps = new PropertyBag();
    nonSyncableProps.setProperty("timeLastUsed", Date.now());
    nonSyncableProps.setProperty("timesUsed", 3);
    Services.logins.modifyLogin(login, nonSyncableProps);

    let noChanges = await engine.pullNewChanges();
    deepEqual(noChanges, {}, "Should not track non-syncable fields");

    let syncableProps = new PropertyBag();
    syncableProps.setProperty("username", "newuser");
    Services.logins.modifyLogin(login, syncableProps);

    let changes = await engine.pullNewChanges();
    deepEqual(Object.keys(changes), [login.guid],
      "Should track syncable fields");
  } finally {
    await cleanup(engine, server);
  }
});

add_task(async function test_ignored_sync_credentials() {
  _("Sync credentials in login manager should be ignored");

  let engine = Service.engineManager.get("passwords");

  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  enableValidationPrefs();

  Svc.Obs.notify("weave:engine:start-tracking");

  try {
    let login = Services.logins.addLogin(new LoginInfo(FXA_PWDMGR_HOST, null,
      FXA_PWDMGR_REALM, "fxa-uid", "creds", "", ""));

    let noChanges = await engine.pullNewChanges();
    deepEqual(noChanges, {}, "Should not track new FxA credentials");

    let props = new PropertyBag();
    props.setProperty("password", "newcreds");
    Services.logins.modifyLogin(login, props);

    noChanges = await engine.pullNewChanges();
    deepEqual(noChanges, {}, "Should not track changes to FxA credentials");
  } finally {
    await cleanup(engine, server);
  }
});

add_task(async function test_password_engine() {
  _("Basic password sync test");

  let engine = Service.engineManager.get("passwords");

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
    await cleanup(engine, server);
  }
});
