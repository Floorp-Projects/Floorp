/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  _("Set up test fixtures.");

  ensureLegacyIdentityManager();
  Service.identity.username = "john@example.com";
  Service.clusterURL = "http://fakebase/";
  let baseUri = "http://fakebase/1.1/foo/storage/";
  let pubUri = baseUri + "keys/pubkey";
  let privUri = baseUri + "keys/privkey";

  Service.identity.syncKey = "abcdeabcdeabcdeabcdeabcdea";
  let keyBundle = Service.identity.syncKeyBundle;

  let engine = Service.clientsEngine;

  try {
    _("Test that serializing client records results in uploadable ascii");
    engine.localID = "ascii";
    engine.localName = "wéävê";

    _("Make sure we have the expected record");
    let record = engine._createRecord("ascii");
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");

    _("Encrypting record...");
    record.encrypt(keyBundle);
    _("Encrypted.");

    let serialized = JSON.stringify(record);
    let checkCount = 0;
    _("Checking for all ASCII:", serialized);
    Array.forEach(serialized, function(ch) {
      let code = ch.charCodeAt(0);
      _("Checking asciiness of '", ch, "'=", code);
      do_check_true(code < 128);
      checkCount++;
    });

    _("Processed", checkCount, "characters out of", serialized.length);
    do_check_eq(checkCount, serialized.length);

    _("Making sure the record still looks like it did before");
    record.decrypt(keyBundle);
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");

    _("Sanity check that creating the record also gives the same");
    record = engine._createRecord("ascii");
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");
  } finally {
    Svc.Prefs.resetBranch("");
  }
}
