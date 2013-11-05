/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/osfile.jsm");

var kf = "keytest.txt"; // not an actual keyfile

function run_test() {
    run_next_test();
}

add_task(function empty_disk() {
    var jslib = Cc["@mozilla.org/url-classifier/jslib;1"]
                .getService().wrappedJSObject;
    this.PROT_UrlCryptoKeyManager = jslib.PROT_UrlCryptoKeyManager;
    yield OS.File.remove(kf);
    do_print("simulate nothing on disk, then get something from server");
    var km = new PROT_UrlCryptoKeyManager(kf, true);
    do_check_false(km.hasKey()); // KM already has key?
    km.maybeLoadOldKey();
    do_check_false(km.hasKey()); // KM loaded nonexistent key?
    yield km.onGetKeyResponse(null);
    do_check_false(km.hasKey()); // KM got key from null response?
    yield km.onGetKeyResponse("");
    do_check_false(km.hasKey()); // KM got key from an empty response?
    yield km.onGetKeyResponse("aslkaslkdf:34:a230\nskdjfaljsie");
    do_check_false(km.hasKey()); // KM got key from garbage response?
    var realResponse = "clientkey:24:zGbaDbx1pxoYe7siZYi8VA==\n" +
                       "wrappedkey:24:MTr1oDt6TSOFQDTvKCWz9PEn";
    yield km.onGetKeyResponse(realResponse);
    // Will have written it to the file as a side effect
    do_check_true(km.hasKey()); // KM could not get key from a real response?
    do_check_eq(km.clientKey_, "zGbaDbx1pxoYe7siZYi8VA=="); // Parsed wrong client key from response?
    do_check_eq(km.wrappedKey_, "MTr1oDt6TSOFQDTvKCWz9PEn"); // Parsed wrong wrapped key from response?

    do_print("simulate something on disk, then get something from server");
    var km = new PROT_UrlCryptoKeyManager(kf, true);
    do_check_false(km.hasKey()); // KM already has key?
    yield km.maybeLoadOldKey();
    do_check_true(km.hasKey()); // KM couldn't load existing key from disk?
    do_check_eq(km.clientKey_ , "zGbaDbx1pxoYe7siZYi8VA=="); // Parsed wrong client key from disk?
    do_check_eq(km.wrappedKey_, "MTr1oDt6TSOFQDTvKCWz9PEn"); // Parsed wrong wrapped key from disk?
    var realResponse2 = "clientkey:24:dtmbEN1kgN/LmuEoYifaFw==\n" +
                        "wrappedkey:24:MTpPH3pnLDKihecOci+0W5dk";
    yield km.onGetKeyResponse(realResponse2);
    do_check_true(km.hasKey()); // KM couldn't replace key from server response?
    do_check_eq(km.clientKey_, "dtmbEN1kgN/LmuEoYifaFw=="); // Replace client key from server failed?
    do_check_eq(km.wrappedKey_, "MTpPH3pnLDKihecOci+0W5dk"); // Replace wrapped key from server failed?

    do_print("check overwriting a key on disk");
    km = new PROT_UrlCryptoKeyManager(kf, true);
    do_check_false(km.hasKey()); // KM already has key?
    yield km.maybeLoadOldKey();
    do_check_true(km.hasKey()); // KM couldn't load existing key from file?
    do_check_eq(km.clientKey_, "dtmbEN1kgN/LmuEoYifaFw=="); // Replace client key on disk failed?
    do_check_eq(km.wrappedKey_, "MTpPH3pnLDKihecOci+0W5dk"); // Replace wrapped key on disk failed?

    do_print("Test that we only fetch at most two getkey's per lifetime of the manager");
    var km = new PROT_UrlCryptoKeyManager(kf, true);
    km.reKey();
    for (var i = 0; i < km.MAX_REKEY_TRIES; i++)
      do_check_true(km.maybeReKey()); // Couldn't rekey?
    do_check_false(km.maybeReKey()); // Rekeyed when max hit?

    yield OS.File.remove(kf);
});
