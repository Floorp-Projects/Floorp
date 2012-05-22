/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");


function CanDecryptEngine() {
  SyncEngine.call(this, "CanDecrypt");
}
CanDecryptEngine.prototype = {
  __proto__: SyncEngine.prototype,

  // Override these methods with mocks for the test
  canDecrypt: function canDecrypt() {
    return true;
  },

  wasWiped: false,
  wipeClient: function wipeClient() {
    this.wasWiped = true;
  }
};
Engines.register(CanDecryptEngine);


function CannotDecryptEngine() {
  SyncEngine.call(this, "CannotDecrypt");
}
CannotDecryptEngine.prototype = {
  __proto__: SyncEngine.prototype,

  // Override these methods with mocks for the test
  canDecrypt: function canDecrypt() {
    return false;
  },

  wasWiped: false,
  wipeClient: function wipeClient() {
    this.wasWiped = true;
  }
};
Engines.register(CannotDecryptEngine);


add_test(function test_withEngineList() {
  try {
    _("Ensure initial scenario.");
    do_check_false(Engines.get("candecrypt").wasWiped);
    do_check_false(Engines.get("cannotdecrypt").wasWiped);

    _("Wipe local engine data.");
    Service.wipeClient(["candecrypt", "cannotdecrypt"]);

    _("Ensure only the engine that can decrypt was wiped.");
    do_check_true(Engines.get("candecrypt").wasWiped);
    do_check_false(Engines.get("cannotdecrypt").wasWiped);
  } finally {
    Engines.get("candecrypt").wasWiped = false;
    Engines.get("cannotdecrypt").wasWiped = false;
    Service.startOver();
  }

  run_next_test();
});

add_test(function test_startOver_clears_keys() {
  generateNewKeys();
  do_check_true(!!CollectionKeys.keyForCollection());
  Service.startOver();
  do_check_false(!!CollectionKeys.keyForCollection());

  run_next_test();
});

add_test(function test_credentials_preserved() {
  _("Ensure that credentials are preserved if client is wiped.");

  // Required for wipeClient().
  Service.clusterURL = TEST_CLUSTER_URL;

  Identity.account = "testaccount";
  Identity.basicPassword = "testpassword";
  let key = Utils.generatePassphrase();
  Identity.syncKey = key;
  Identity.persistCredentials();

  // Simulate passwords engine wipe without all the overhead. To do this
  // properly would require extra test infrastructure.
  Services.logins.removeAllLogins();
  Service.wipeClient();

  let id = new IdentityManager();
  do_check_eq(id.account, "testaccount");
  do_check_eq(id.basicPassword, "testpassword");
  do_check_eq(id.syncKey, key);

  Service.startOver();

  run_next_test();
});

function run_test() {
  initTestLogging();

  run_next_test();
}
