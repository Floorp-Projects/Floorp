/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Set up test fixtures.");

  Identity.username = "john@example.com";
  Svc.Prefs.set("clusterURL", "http://fakebase/");
  let baseUri = "http://fakebase/1.1/foo/storage/";
  let pubUri = baseUri + "keys/pubkey";
  let privUri = baseUri + "keys/privkey";

  Identity.syncKey = "abcdeabcdeabcdeabcdeabcdea";
  let keyBundle = Identity.syncKeyBundle;

  try {
    _("Test that serializing client records results in uploadable ascii");
    Clients.localID = "ascii";
    Clients.localName = "wéävê";

    _("Make sure we have the expected record");
    let record = Clients._createRecord("ascii");
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
    record = Clients._createRecord("ascii");
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");
  } finally {
    Svc.Prefs.resetBranch("");
  }
}
