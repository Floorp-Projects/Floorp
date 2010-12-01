let atob = Cu.import("resource://services-sync/util.js").atob;
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/base_records/crypto.js");

/**
 * Testing the SHA256-HMAC key derivation process against st3fan's implementation
 * in Firefox Home.
 */
function run_test() {
  
  // Test the production of keys from a sync key.
  let bundle = new SyncKeyBundle(PWDMGR_PASSPHRASE_REALM, "st3fan", "q7ynpwq7vsc9m34hankbyi3s3i");
  
  // These should be compared to the results from Home, as they once were.
  let e = "3fe2d3743fe03d4f460ce2405ec189e68dfd7e42c97d50fab9bda3761263cc87";
  let h = "bf05f720423d297e8fd55faee7cdeaf32aa15cfb6e56115268c9c326b999795a";
  
  // The encryption key is stored as base64 for handing off to WeaveCrypto.
  let realE = Utils.bytesAsHex(atob(bundle.encryptionKey));
  let realH = Utils.bytesAsHex(bundle.hmacKey);
  
  _("Real E: " + realE);
  _("Real H: " + realH);
  do_check_eq(realH, h);
  do_check_eq(realE, e);
}
