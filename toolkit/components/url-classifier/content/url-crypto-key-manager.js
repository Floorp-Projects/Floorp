/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// This file implements the tricky business of managing the keys for our 
// URL encryption. The protocol is:
//
// - Server generates secret key K_S
// - Client starts up and requests a new key K_C from the server via HTTPS
// - Server generates K_C and WrappedKey, which is K_C encrypted with K_S
// - Server resonse with K_C and WrappedKey
// - When client wants to encrypt a URL, it encrypts it with K_C and sends
//   the encrypted URL along with WrappedKey
// - Server decrypts WrappedKey with K_S to get K_C, and the URL with K_C
//
// This is, however, trickier than it sounds for two reasons. First,
// we want to keep the number of HTTPS requests to an aboslute minimum
// (like 1 or 2 per browser session). Second, the HTTPS request at
// startup might fail, for example the user might be offline or a URL
// fetch might need to be issued before the HTTPS request has
// completed.
//
// We implement the following policy:
// 
// - The extension will issue at most two HTTPS getkey requests per session
// - The extension will issue one HTTPS getkey request at startup
// - The extension will serialize to disk any key it gets
// - The extension will fall back on this serialized key until it has a
//   fresh key
// - The front-end can respond with a flag in a lookup request that tells
//   the client to re-key. The client will issue a new HTTPS getkey request
//   at this time if it has only issued one before

// We store the user key in this file.  The key can be used to verify signed
// server updates.
const kKeyFilename = "kf.txt";

// If we don't have a key, we can get one at this url.
// XXX We shouldn't be referencing browser.safebrowsing. from here.  This
// should be an constructor param or settable some other way.
const kGetKeyUrl = "browser.safebrowsing.provider.0.keyURL";

/**
 * A key manager for UrlCrypto. There should be exactly one of these
 * per appplication, and all UrlCrypto's should share it. This is
 * currently implemented by having the manager attach itself to the
 * UrlCrypto's prototype at startup. We could've opted for a global
 * instead, but I like this better, even though it is spooky action
 * at a distance.
 *
 * @param opt_keyFilename String containing the name of the 
 *                        file we should serialize keys to/from. Used
 *                        mostly for testing.
 *
 * @param opt_testing Boolean indicating whether we are testing. If we 
 *                    are, then we skip trying to read the old key from
 *                    file and automatically trying to rekey; presumably
 *                    the tester will drive these manually.
 *
 * @constructor
 */
function PROT_UrlCryptoKeyManager(opt_keyFilename, opt_testing) {
  this.debugZone = "urlcryptokeymanager";
  this.testing_ = !!opt_testing;
  this.base64_ = new G_Base64();
  this.clientKey_ = null;          // Base64-encoded, as fetched from server
  this.clientKeyArray_ = null;     // Base64-decoded into an array of numbers
  this.wrappedKey_ = null;         // Opaque websafe base64-encoded server key
  this.rekeyTries_ = 0;

  this.keyFilename_ = opt_keyFilename ? 
                      opt_keyFilename : kKeyFilename;

  // Convenience properties
  this.MAX_REKEY_TRIES = PROT_UrlCryptoKeyManager.MAX_REKEY_TRIES;
  this.CLIENT_KEY_NAME = PROT_UrlCryptoKeyManager.CLIENT_KEY_NAME;
  this.WRAPPED_KEY_NAME = PROT_UrlCryptoKeyManager.WRAPPED_KEY_NAME;

  if (!this.testing_) {
    G_Assert(this, !PROT_UrlCrypto.prototype.manager_,
             "Already have manager?");
    PROT_UrlCrypto.prototype.manager_ = this;

    this.maybeLoadOldKey();
    this.reKey();
  }
}

// Do ***** NOT ***** set this higher; HTTPS is expensive
PROT_UrlCryptoKeyManager.MAX_REKEY_TRIES = 2;

// These are the names the server will respond with in protocol4 format
PROT_UrlCryptoKeyManager.CLIENT_KEY_NAME = "clientkey";
PROT_UrlCryptoKeyManager.WRAPPED_KEY_NAME = "wrappedkey";


/**
 * Called by a UrlCrypto to get the current K_C
 *
 * @returns Array of numbers making up the client key or null if we 
 *          have no key
 */
PROT_UrlCryptoKeyManager.prototype.getClientKeyArray = function() {
  return this.clientKeyArray_;
}

/**
 * Called by a UrlCrypto to get WrappedKey
 *
 * @returns Opaque base64-encoded WrappedKey or null if we haven't
 *          gotten one
 */
PROT_UrlCryptoKeyManager.prototype.getWrappedKey = function() {
  return this.wrappedKey_;
}

/**
 * Tell the manager to re-key. For safety, this method still obeys the
 * max-tries limit. Clients should generally use maybeReKey() if they
 * want to try a re-keying: it's an error to call reKey() after we've
 * hit max-tries, but not an error to call maybeReKey().
 */
PROT_UrlCryptoKeyManager.prototype.reKey = function() {
  
  if (this.rekeyTries_ > this.MAX_REKEY_TRIES)
    throw new Error("Have already rekeyed " + this.rekeyTries_ + " times");

  this.rekeyTries_++;

  G_Debug(this, "Attempting to re-key");
  var prefs = new G_Preferences();
  var url = prefs.getPref(kGetKeyUrl, null);
  if (!this.testing_ && url)
    (new PROT_XMLFetcher()).get(url, 
                                BindToObject(this.onGetKeyResponse, this));
}

/**
 * Try to re-key if we haven't already hit our limit. It's OK to call
 * this method multiple times, even if we've already tried to rekey
 * more than the max. It will simply refuse to do so.
 *
 * @returns Boolean indicating if it actually issued a rekey request (that
 *          is, if we haven' already hit the max)
 */
PROT_UrlCryptoKeyManager.prototype.maybeReKey = function() {
  if (this.rekeyTries_ > this.MAX_REKEY_TRIES) {
    G_Debug(this, "Not re-keying; already at max");
    return false;
  }

  this.reKey();
  return true;
}

/**
 * @returns Boolean indicating if we have a key we can use 
 */
PROT_UrlCryptoKeyManager.prototype.hasKey_ = function() {
  return this.clientKey_ != null && this.wrappedKey_ != null;
}

/**
 * Set a new key and serialize it to disk.
 *
 * @param clientKey String containing the base64-encoded client key 
 *                  we wish to use
 *
 * @param wrappedKey String containing the opaque base64-encoded WrappedKey
 *                   the server gave us (i.e., K_C encrypted with K_S)
 */
PROT_UrlCryptoKeyManager.prototype.replaceKey_ = function(clientKey, 
                                                          wrappedKey) {
  if (this.clientKey_)
    G_Debug(this, "Replacing " + this.clientKey_ + " with " + clientKey);

  this.clientKey_ = clientKey;
  this.clientKeyArray_ = this.base64_.decodeString(this.clientKey_);
  this.wrappedKey_ = wrappedKey;

  this.serializeKey_(this.clientKey_, this.wrappedKey_);
}

/**
 * Try to write the key to disk so we can fall back on it. Fail
 * silently if we cannot. The keys are serialized in protocol4 format.
 *
 * @returns Boolean indicating whether we succeeded in serializing
 */
PROT_UrlCryptoKeyManager.prototype.serializeKey_ = function() {

  var map = {};
  map[this.CLIENT_KEY_NAME] = this.clientKey_;
  map[this.WRAPPED_KEY_NAME] = this.wrappedKey_;
  
  try {  

    var appDir = new PROT_ApplicationDirectory();
    if (!appDir.exists())
      appDir.create();
    var keyfile = appDir.getAppDirFileInterface();
    keyfile.append(this.keyFilename_);
    G_FileWriter.writeAll(keyfile, (new G_Protocol4Parser).serialize(map));
    return true;

  } catch(e) {

    G_Error(this, "Failed to serialize new key: " + e);
    return false;

  }
}

/**
 * Invoked when we've received a protocol4 response to our getkey
 * request. Try to parse it and set this key as the new one if we can.
 *
 *  @param responseText String containing the protocol4 getkey response
 */ 
PROT_UrlCryptoKeyManager.prototype.onGetKeyResponse = function(responseText) {

  var response = (new G_Protocol4Parser).parse(responseText);
  var clientKey = response[this.CLIENT_KEY_NAME];
  var wrappedKey = response[this.WRAPPED_KEY_NAME];

  if (response && clientKey && wrappedKey) {
    G_Debug(this, "Got new key from: " + responseText);
    this.replaceKey_(clientKey, wrappedKey);
  } else {
    G_Debug(this, "Not a valid response for /getkey");
  }
}

/**
 * Attempt to read a key we've previously serialized from disk, so
 * that we can fall back on it in case we can't get one from the
 * server. If we get a key, only use it if we don't already have one
 * (i.e., if our startup HTTPS request died or hasn't yet completed).
 *
 * This method should be invoked early, like when the user's profile
 * becomes available.
 */ 
PROT_UrlCryptoKeyManager.prototype.maybeLoadOldKey = function() {
  
  var oldKey = null;
  try {  
    var appDir = new PROT_ApplicationDirectory();
    var keyfile = appDir.getAppDirFileInterface();
    keyfile.append(this.keyFilename_);
    if (keyfile.exists())
      oldKey = G_FileReader.readAll(keyfile);
  } catch(e) {
    G_Debug(this, "Caught " + e + " trying to read keyfile");
    return;
  }
   
  if (!oldKey) {
    G_Debug(this, "Couldn't find old key.");
    return;
  }

  oldKey = (new G_Protocol4Parser).parse(oldKey);
  var clientKey = oldKey[this.CLIENT_KEY_NAME];
  var wrappedKey = oldKey[this.WRAPPED_KEY_NAME];

  if (oldKey && clientKey && wrappedKey && !this.hasKey_()) {
    G_Debug(this, "Read old key from disk.");
    this.replaceKey_(clientKey, wrappedKey);
  }
}


#ifdef DEBUG
/**
 * Cheesey tests
 */
function TEST_PROT_UrlCryptoKeyManager() {
  if (G_GDEBUG) {
    var z = "urlcryptokeymanager UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    // Let's not clobber any real keyfile out there
    var kf = "keytest.txt";

    // Let's be able to clean up after ourselves
    function removeTestFile(f) {
      var appDir = new PROT_ApplicationDirectory();
      var file = appDir.getAppDirFileInterface();
      file.append(f);
      if (file.exists())
        file.remove(false /* do not recurse */);
    };
    removeTestFile(kf);

    var km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);

    // CASE: simulate nothing on disk, then get something from server

    G_Assert(z, !km.hasKey_(), "KM already has key?");
    km.maybeLoadOldKey();
    G_Assert(z, !km.hasKey_(), "KM loaded non-existent key?");
    km.onGetKeyResponse(null);
    G_Assert(z, !km.hasKey_(), "KM got key from null response?");
    km.onGetKeyResponse("");
    G_Assert(z, !km.hasKey_(), "KM got key from empty response?");
    km.onGetKeyResponse("aslkaslkdf:34:a230\nskdjfaljsie");
    G_Assert(z, !km.hasKey_(), "KM got key from garbage response?");
    
    var realResponse = "clientkey:24:zGbaDbx1pxoYe7siZYi8VA==\n" +
                       "wrappedkey:24:MTr1oDt6TSOFQDTvKCWz9PEn";
    km.onGetKeyResponse(realResponse);
    // Will have written it to file as a side effect
    G_Assert(z, km.hasKey_(), "KM couldn't get key from real response?");
    G_Assert(z, km.clientKey_ == "zGbaDbx1pxoYe7siZYi8VA==", 
             "Parsed wrong client key from response?");
    G_Assert(z, km.wrappedKey_ == "MTr1oDt6TSOFQDTvKCWz9PEn", 
             "Parsed wrong wrapped key from response?");

    // CASE: simulate something on disk, then get something from server
    
    km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);
    G_Assert(z, !km.hasKey_(), "KM already has key?");
    km.maybeLoadOldKey();
    G_Assert(z, km.hasKey_(), "KM couldn't load existing key from disk?");
    G_Assert(z, km.clientKey_ == "zGbaDbx1pxoYe7siZYi8VA==", 
             "Parsed wrong client key from disk?");
    G_Assert(z, km.wrappedKey_ == "MTr1oDt6TSOFQDTvKCWz9PEn", 
             "Parsed wrong wrapped key from disk?");
    var realResponse2 = "clientkey:24:dtmbEN1kgN/LmuEoYifaFw==\n" +
                        "wrappedkey:24:MTpPH3pnLDKihecOci+0W5dk";
    km.onGetKeyResponse(realResponse2);
    // Will have written it to disk
    G_Assert(z, km.hasKey_(), "KM couldn't replace key from server response?");
    G_Assert(z, km.clientKey_ == "dtmbEN1kgN/LmuEoYifaFw==",
             "Replace client key from server failed?");
    G_Assert(z, km.wrappedKey_ == "MTpPH3pnLDKihecOci+0W5dk", 
             "Replace wrapped key from server failed?");

    // CASE: check overwriting a key on disk

    km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);
    G_Assert(z, !km.hasKey_(), "KM already has key?");
    km.maybeLoadOldKey();
    G_Assert(z, km.hasKey_(), "KM couldn't load existing key from disk?");
    G_Assert(z, km.clientKey_ == "dtmbEN1kgN/LmuEoYifaFw==",
             "Replace client on from disk failed?");
    G_Assert(z, km.wrappedKey_ == "MTpPH3pnLDKihecOci+0W5dk", 
             "Replace wrapped key on disk failed?");

    // Test that we only fetch at most two getkey's per lifetime of the manager

    km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);
    km.reKey();
    for (var i = 0; i < km.MAX_REKEY_TRIES; i++)
      G_Assert(z, km.maybeReKey(), "Couldn't rekey?");
    G_Assert(z, !km.maybeReKey(), "Rekeyed when max hit");
    
    removeTestFile(kf);

    G_Debug(z, "PASSED");  

  }
}
#endif
