/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

add_task(async function test_clients_escape() {
  _("Set up test fixtures.");

  await configureIdentity();
  let keyBundle = Service.identity.syncKeyBundle;

  let engine = Service.clientsEngine;

  try {
    _("Test that serializing client records results in uploadable ascii");
    engine.localID = "ascii";
    engine.localName = "wéävê";

    _("Make sure we have the expected record");
    let record = await engine._createRecord("ascii");
    Assert.equal(record.id, "ascii");
    Assert.equal(record.name, "wéävê");

    _("Encrypting record...");
    await record.encrypt(keyBundle);
    _("Encrypted.");

    let serialized = JSON.stringify(record);
    let checkCount = 0;
    _("Checking for all ASCII:", serialized);
    Array.forEach(serialized, function(ch) {
      let code = ch.charCodeAt(0);
      _("Checking asciiness of '", ch, "'=", code);
      Assert.ok(code < 128);
      checkCount++;
    });

    _("Processed", checkCount, "characters out of", serialized.length);
    Assert.equal(checkCount, serialized.length);

    _("Making sure the record still looks like it did before");
    await record.decrypt(keyBundle);
    Assert.equal(record.id, "ascii");
    Assert.equal(record.name, "wéävê");

    _("Sanity check that creating the record also gives the same");
    record = await engine._createRecord("ascii");
    Assert.equal(record.id, "ascii");
    Assert.equal(record.name, "wéävê");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});
