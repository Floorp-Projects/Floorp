# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
// - Firefox will issue at most two HTTPS getkey requests per session
// - Firefox will issue one HTTPS getkey request at startup if more than 24
//   hours has passed since the last getkey request.
// - Firefox will serialize to disk any key it gets
// - Firefox will fall back on this serialized key until it has a
//   fresh key
// - The front-end can respond with a flag in a lookup request that tells
//   the client to re-key. Firefox will issue a new HTTPS getkey request
//   at this time if it has only issued one before

// We store the user key in this file.  The key can be used to verify signed
// server updates.
const kKeyFilename = "urlclassifierkey3.txt";

/**
 * A key manager for UrlCrypto. There should be exactly one of these
 * per appplication, and all UrlCrypto's should share it. This is
 * currently implemented by having the manager attach itself to the
 * UrlCrypto's prototype at startup. We could've opted for a global
 * instead, but I like this better, even though it is spooky action
 * at a distance.
 * XXX: Should be an XPCOM service
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
  this.clientKey_ = null;          // Base64-encoded, as fetched from server
  this.clientKeyArray_ = null;     // Base64-decoded into an array of numbers
  this.wrappedKey_ = null;         // Opaque websafe base64-encoded server key
  this.rekeyTries_ = 0;
  this.updating_ = false;

  // Don't do anything until keyUrl_ is set.
  this.keyUrl_ = null;

  this.keyFilename_ = opt_keyFilename ? 
                      opt_keyFilename : kKeyFilename;

  this.onNewKey_ = null;

  // Convenience properties
  this.MAX_REKEY_TRIES = PROT_UrlCryptoKeyManager.MAX_REKEY_TRIES;
  this.CLIENT_KEY_NAME = PROT_UrlCryptoKeyManager.CLIENT_KEY_NAME;
  this.WRAPPED_KEY_NAME = PROT_UrlCryptoKeyManager.WRAPPED_KEY_NAME;

  if (!this.testing_) {
    this.maybeLoadOldKey();
  }
}

// Do ***** NOT ***** set this higher; HTTPS is expensive
PROT_UrlCryptoKeyManager.MAX_REKEY_TRIES = 2;

// Base pref for keeping track of when we updated our key.
// We store the time as seconds since the epoch.
PROT_UrlCryptoKeyManager.NEXT_REKEY_PREF = "urlclassifier.keyupdatetime.";

// Once every 30 days (interval in seconds)
PROT_UrlCryptoKeyManager.KEY_MIN_UPDATE_TIME = 30 * 24 * 60 * 60;

// These are the names the server will respond with in protocol4 format
PROT_UrlCryptoKeyManager.CLIENT_KEY_NAME = "clientkey";
PROT_UrlCryptoKeyManager.WRAPPED_KEY_NAME = "wrappedkey";

/**
 * Called to get ClientKey
 * @returns urlsafe-base64-encoded client key or null if we haven't gotten one.
 */
PROT_UrlCryptoKeyManager.prototype.getClientKey = function() {
  return this.clientKey_;
}

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
 * Change the key url.  When we do this, we go ahead and rekey.
 * @param keyUrl String
 */
PROT_UrlCryptoKeyManager.prototype.setKeyUrl = function(keyUrl) {
  // If it's the same key url, do nothing.
  if (keyUrl == this.keyUrl_)
    return;

  this.keyUrl_ = keyUrl;
  this.rekeyTries_ = 0;

  // Check to see if we should make a new getkey request.
  var prefs = new G_Preferences(PROT_UrlCryptoKeyManager.NEXT_REKEY_PREF);
  var nextRekey = prefs.getPref(this.getPrefName_(this.keyUrl_), 0);
  if (nextRekey < parseInt(Date.now() / 1000, 10)) {
    this.reKey();
  }
}

/**
 * Given a url, return the pref value to use (pref contains last update time).
 * We basically use the url up until query parameters.  This avoids duplicate
 * pref entries as version number changes over time.
 * @param url String getkey URL
 */
PROT_UrlCryptoKeyManager.prototype.getPrefName_ = function(url) {
  var queryParam = url.indexOf("?");
  if (queryParam != -1) {
    return url.substring(0, queryParam);
  }
  return url;
}

/**
 * Tell the manager to re-key. For safety, this method still obeys the
 * max-tries limit. Clients should generally use maybeReKey() if they
 * want to try a re-keying: it's an error to call reKey() after we've
 * hit max-tries, but not an error to call maybeReKey().
 */
PROT_UrlCryptoKeyManager.prototype.reKey = function() {
  if (this.updating_) {
    G_Debug(this, "Already re-keying, ignoring this request");
    return true;
  }

  if (this.rekeyTries_ > this.MAX_REKEY_TRIES)
    throw new Error("Have already rekeyed " + this.rekeyTries_ + " times");

  this.rekeyTries_++;

  G_Debug(this, "Attempting to re-key");
  // If the keyUrl isn't set, we don't do anything.
  if (!this.testing_ && this.keyUrl_) {
    this.fetcher_ = new PROT_XMLFetcher();
    this.fetcher_.get(this.keyUrl_, BindToObject(this.onGetKeyResponse, this));
    this.updating_ = true;

    // Calculate the next time we're allowed to re-key.
    var prefs = new G_Preferences(PROT_UrlCryptoKeyManager.NEXT_REKEY_PREF);
    var nextRekey = parseInt(Date.now() / 1000, 10)
                  + PROT_UrlCryptoKeyManager.KEY_MIN_UPDATE_TIME;
    prefs.setPref(this.getPrefName_(this.keyUrl_), nextRekey);
  }
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
 * Drop the existing set of keys.  Resets the rekeyTries variable to
 * allow a rekey to succeed.
 */
PROT_UrlCryptoKeyManager.prototype.dropKey = function() {
  this.rekeyTries_ = 0;
  this.replaceKey_(null, null);
}

/**
 * @returns Boolean indicating if we have a key we can use 
 */
PROT_UrlCryptoKeyManager.prototype.hasKey = function() {
  return this.clientKey_ != null && this.wrappedKey_ != null;
}

PROT_UrlCryptoKeyManager.prototype.unUrlSafe = function(key)
{
    return key ? key.replace(/-/g, "+").replace(/_/g, "/") : "";
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
  this.clientKeyArray_ = Array.map(atob(this.unUrlSafe(clientKey)),
                                   function(c) { return c.charCodeAt(0); });
  this.wrappedKey_ = wrappedKey;

  this.serializeKey_(this.clientKey_, this.wrappedKey_);

  if (this.onNewKey_) {
    this.onNewKey_();
  }
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

    var keyfile = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties)
                 .get("ProfD", Ci.nsILocalFile); /* profile directory */
    keyfile.append(this.keyFilename_);

    if (!this.clientKey_ || !this.wrappedKey_) {
      keyfile.remove(true);
      return;
    }

    var data = (new G_Protocol4Parser()).serialize(map);

    try {
      var stream = Cc["@mozilla.org/network/file-output-stream;1"]
                   .createInstance(Ci.nsIFileOutputStream);
      stream.init(keyfile,
                  0x02 | 0x08 | 0x20 /* PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE */,
                  -1 /* default perms */, 0 /* no special behavior */);
      stream.write(data, data.length);
    } finally {
      stream.close();
    }
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

  this.updating_ = false;
  this.fetcher_ = null;

  if (response && clientKey && wrappedKey) {
    G_Debug(this, "Got new key from: " + responseText);
    this.replaceKey_(clientKey, wrappedKey);
  } else {
    G_Debug(this, "Not a valid response for /newkey");
  }
}

/**
 * Set the callback to be called whenever we get a new key.
 *
 * @param callback The callback.
 */
PROT_UrlCryptoKeyManager.prototype.onNewKey = function(callback) 
{
  this.onNewKey_ = callback;
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
    var keyfile = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties)
                 .get("ProfD", Ci.nsILocalFile); /* profile directory */
    keyfile.append(this.keyFilename_);
    if (keyfile.exists()) {
      try {
        var fis = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
        fis.init(keyfile, 0x01 /* PR_RDONLY */, 0444, 0);
        var stream = Cc["@mozilla.org/scriptableinputstream;1"]
                     .createInstance(Ci.nsIScriptableInputStream);
        stream.init(fis);
        oldKey = stream.read(stream.available());
      } finally {
        if (stream)
          stream.close();
      }
    }
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

  if (oldKey && clientKey && wrappedKey && !this.hasKey()) {
    G_Debug(this, "Read old key from disk.");
    this.replaceKey_(clientKey, wrappedKey);
  }
}

PROT_UrlCryptoKeyManager.prototype.shutdown = function() {
  if (this.fetcher_) {
    this.fetcher_.cancel();
    this.fetcher_ = null;
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
      var file = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties)
                 .get("ProfD", Ci.nsILocalFile); /* profile directory */
      file.append(f);
      if (file.exists())
        file.remove(false /* do not recurse */);
    };
    removeTestFile(kf);

    var km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);

    // CASE: simulate nothing on disk, then get something from server

    G_Assert(z, !km.hasKey(), "KM already has key?");
    km.maybeLoadOldKey();
    G_Assert(z, !km.hasKey(), "KM loaded nonexistent key?");
    km.onGetKeyResponse(null);
    G_Assert(z, !km.hasKey(), "KM got key from null response?");
    km.onGetKeyResponse("");
    G_Assert(z, !km.hasKey(), "KM got key from empty response?");
    km.onGetKeyResponse("aslkaslkdf:34:a230\nskdjfaljsie");
    G_Assert(z, !km.hasKey(), "KM got key from garbage response?");
    
    var realResponse = "clientkey:24:zGbaDbx1pxoYe7siZYi8VA==\n" +
                       "wrappedkey:24:MTr1oDt6TSOFQDTvKCWz9PEn";
    km.onGetKeyResponse(realResponse);
    // Will have written it to file as a side effect
    G_Assert(z, km.hasKey(), "KM couldn't get key from real response?");
    G_Assert(z, km.clientKey_ == "zGbaDbx1pxoYe7siZYi8VA==", 
             "Parsed wrong client key from response?");
    G_Assert(z, km.wrappedKey_ == "MTr1oDt6TSOFQDTvKCWz9PEn", 
             "Parsed wrong wrapped key from response?");

    // CASE: simulate something on disk, then get something from server
    
    km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);
    G_Assert(z, !km.hasKey(), "KM already has key?");
    km.maybeLoadOldKey();
    G_Assert(z, km.hasKey(), "KM couldn't load existing key from disk?");
    G_Assert(z, km.clientKey_ == "zGbaDbx1pxoYe7siZYi8VA==", 
             "Parsed wrong client key from disk?");
    G_Assert(z, km.wrappedKey_ == "MTr1oDt6TSOFQDTvKCWz9PEn", 
             "Parsed wrong wrapped key from disk?");
    var realResponse2 = "clientkey:24:dtmbEN1kgN/LmuEoYifaFw==\n" +
                        "wrappedkey:24:MTpPH3pnLDKihecOci+0W5dk";
    km.onGetKeyResponse(realResponse2);
    // Will have written it to disk
    G_Assert(z, km.hasKey(), "KM couldn't replace key from server response?");
    G_Assert(z, km.clientKey_ == "dtmbEN1kgN/LmuEoYifaFw==",
             "Replace client key from server failed?");
    G_Assert(z, km.wrappedKey == "MTpPH3pnLDKihecOci+0W5dk", 
             "Replace wrapped key from server failed?");

    // CASE: check overwriting a key on disk

    km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);
    G_Assert(z, !km.hasKey(), "KM already has key?");
    km.maybeLoadOldKey();
    G_Assert(z, km.hasKey(), "KM couldn't load existing key from disk?");
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
