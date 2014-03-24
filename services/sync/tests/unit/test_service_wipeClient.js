/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Service.engineManager.clear();

function CanDecryptEngine() {
  SyncEngine.call(this, "CanDecrypt", Service);
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
Service.engineManager.register(CanDecryptEngine);


function CannotDecryptEngine() {
  SyncEngine.call(this, "CannotDecrypt", Service);
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
Service.engineManager.register(CannotDecryptEngine);


add_test(function test_withEngineList() {
  try {
    _("Ensure initial scenario.");
    do_check_false(Service.engineManager.get("candecrypt").wasWiped);
    do_check_false(Service.engineManager.get("cannotdecrypt").wasWiped);

    _("Wipe local engine data.");
    Service.wipeClient(["candecrypt", "cannotdecrypt"]);

    _("Ensure only the engine that can decrypt was wiped.");
    do_check_true(Service.engineManager.get("candecrypt").wasWiped);
    do_check_false(Service.engineManager.get("cannotdecrypt").wasWiped);
  } finally {
    Service.engineManager.get("candecrypt").wasWiped = false;
    Service.engineManager.get("cannotdecrypt").wasWiped = false;
    Service.startOver();
  }

  run_next_test();
});

add_test(function test_startOver_clears_keys() {
  generateNewKeys(Service.collectionKeys);
  do_check_true(!!Service.collectionKeys.keyForCollection());
  Service.startOver();
  do_check_false(!!Service.collectionKeys.keyForCollection());

  run_next_test();
});

add_test(function test_credentials_preserved() {
  _("Ensure that credentials are preserved if client is wiped.");

  // Required for wipeClient().
  ensureLegacyIdentityManager();
  Service.identity.account = "testaccount";
  Service.identity.basicPassword = "testpassword";
  Service.clusterURL = "http://dummy:9000/";
  let key = Utils.generatePassphrase();
  Service.identity.syncKey = key;
  Service.identity.persistCredentials();

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
