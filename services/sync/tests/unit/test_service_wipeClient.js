/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function CanDecryptEngine() {
  SyncEngine.call(this, "CanDecrypt", Service);
}
CanDecryptEngine.prototype = {
  __proto__: SyncEngine.prototype,

  // Override these methods with mocks for the test
  async canDecrypt() {
    return true;
  },

  wasWiped: false,
  async wipeClient() {
    this.wasWiped = true;
  }
};


function CannotDecryptEngine() {
  SyncEngine.call(this, "CannotDecrypt", Service);
}
CannotDecryptEngine.prototype = {
  __proto__: SyncEngine.prototype,

  // Override these methods with mocks for the test
  async canDecrypt() {
    return false;
  },

  wasWiped: false,
  async wipeClient() {
    this.wasWiped = true;
  }
};

let canDecryptEngine;
let cannotDecryptEngine;

add_task(async function setup() {
  initTestLogging();
  Service.engineManager.clear();

  await Service.engineManager.register(CanDecryptEngine);
  await Service.engineManager.register(CannotDecryptEngine);
  canDecryptEngine = Service.engineManager.get("candecrypt");
  cannotDecryptEngine = Service.engineManager.get("cannotdecrypt");
});

add_task(async function test_withEngineList() {
  try {
    _("Ensure initial scenario.");
    Assert.ok(!canDecryptEngine.wasWiped);
    Assert.ok(!cannotDecryptEngine.wasWiped);

    _("Wipe local engine data.");
    await Service.wipeClient(["candecrypt", "cannotdecrypt"]);

    _("Ensure only the engine that can decrypt was wiped.");
    Assert.ok(canDecryptEngine.wasWiped);
    Assert.ok(!cannotDecryptEngine.wasWiped);
  } finally {
    canDecryptEngine.wasWiped = false;
    cannotDecryptEngine.wasWiped = false;
    await Service.startOver();
  }
});

add_task(async function test_startOver_clears_keys() {
  await generateNewKeys(Service.collectionKeys);
  Assert.ok(!!Service.collectionKeys.keyForCollection());
  await Service.startOver();
  Assert.ok(!Service.collectionKeys.keyForCollection());
});
