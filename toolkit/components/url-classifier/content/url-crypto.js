# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Google Safe Browsing.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Fritz Schneider <fritz@google.com> (original author)
#   Monica Chew <mmc@google.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


// This file implements our query param encryption. You hand it a set
// of query params, and it will hand you a set of (maybe) encrypted
// query params back. It takes the query params you give it, 
// encodes and encrypts them into a encrypted query param, and adds
// the extra query params the server will need to decrypt them
// (e.g., the version of encryption and the decryption key).
// 
// The key manager provides the keys we need; this class just focuses
// on encrypting query params. See the url crypto key manager for
// details of our protocol, but essentially encryption is
// RC4_key(input) with key == MD5(K_C || nonce) where nonce is a
// 32-bit integer appended big-endian and K_C is the client's key.
//
// If for some reason we don't have an encryption key, encrypting is the 
// identity function.

/**
 * This class knows how to encrypt query parameters that will be
 * understood by the lookupserver.
 * 
 * @constructor
 */
function PROT_UrlCrypto() {
  this.debugZone = "urlcrypto";
  this.hasher_ = new G_CryptoHasher();
  this.streamCipher_ = Cc["@mozilla.org/security/streamcipher;1"]
                       .createInstance(Ci.nsIStreamCipher);

  if (!this.manager_) {
    // Create a UrlCryptoKeyManager to reads keys from profile directory if
    // one doesn't already exist.  UrlCryptoKeyManager puts a reference to
    // itself on PROT_UrlCrypto.prototype (this also prevents garbage
    // collection).
    new PROT_UrlCryptoKeyManager();
  }

  // Convenience properties
  this.VERSION = PROT_UrlCrypto.VERSION;
  this.RC4_DISCARD_BYTES = PROT_UrlCrypto.RC4_DISCARD_BYTES;
  this.VERSION_QUERY_PARAM_NAME = PROT_UrlCrypto.QPS.VERSION_QUERY_PARAM_NAME;
  this.ENCRYPTED_PARAMS_PARAM_NAME = 
    PROT_UrlCrypto.QPS.ENCRYPTED_PARAMS_PARAM_NAME;
  this.COUNT_QUERY_PARAM_NAME = PROT_UrlCrypto.QPS.COUNT_QUERY_PARAM_NAME;
  this.WRAPPEDKEY_QUERY_PARAM_NAME = 
    PROT_UrlCrypto.QPS.WRAPPEDKEY_QUERY_PARAM_NAME;

  // Properties for computing macs
  this.macer_ = new G_CryptoHasher(); // don't use hasher_
  this.macInitialized_ = false;
  // Separator to prevent leakage between key and data when computing mac
  this.separator_ = ":coolgoog:";
  this.separatorArray_ = Array.map(this.separator_,
                                   function(c) { return c.charCodeAt(0); });
}

// The version of encryption we implement
PROT_UrlCrypto.VERSION = "1";

PROT_UrlCrypto.RC4_DISCARD_BYTES = 1600;

// The query params are we going to send to let the server know what is
// encrypted, and how
PROT_UrlCrypto.QPS = {};
PROT_UrlCrypto.QPS.VERSION_QUERY_PARAM_NAME = "encver";
PROT_UrlCrypto.QPS.ENCRYPTED_PARAMS_PARAM_NAME = "encparams";
PROT_UrlCrypto.QPS.COUNT_QUERY_PARAM_NAME = "nonce";
PROT_UrlCrypto.QPS.WRAPPEDKEY_QUERY_PARAM_NAME = "wrkey";

/**
 * @returns Reference to the keymanager (if one exists), else undefined
 */
PROT_UrlCrypto.prototype.getManager = function() {
  return this.manager_;
}

/**
 * Helper method that takes a map of query params (param name ->
 * value) and turns them into a query string. Note that it encodes
 * the values as it writes the string.
 *
 * @param params Object (map) of query names to values. Values should
 *               not be uriencoded.
 *
 * @returns String of query params from the map. Values will be uri
 *          encoded
 */
PROT_UrlCrypto.prototype.appendParams_ = function(params) {
  var queryString = "";
  for (var param in params)
    queryString += "&" + param + "=" + encodeURIComponent(params[param]);
                   
  return queryString;
}

/**
 * Encrypt a set of query params if we can. If we can, we return a new
 * set of query params that should be added to a query string. The set
 * of query params WILL BE different than the input query params if we
 * can encrypt (e.g., there will be extra query params with meta-
 * information such as the version of encryption we're using). If we
 * can't encrypt, we just return the query params we're passed.
 *
 * @param params Object (map) of query param names to values. Values should
 *               not be uriencoded.
 *
 * @returns Object (map) of query param names to values. Values are NOT
 *          uriencoded; the caller should encode them as it writes them
 *          to a proper query string.
 */
PROT_UrlCrypto.prototype.maybeCryptParams = function(params) {
  if (!this.manager_)
    throw new Error("Need a key manager for UrlCrypto");
  if (typeof params != "object")
    throw new Error("params is an associative array of name/value params");

  var clientKeyArray = this.manager_.getClientKeyArray();
  var wrappedKey = this.manager_.getWrappedKey();

  // No keys? Can't encrypt. Damn.
  if (!clientKeyArray || !wrappedKey) {
    G_Debug(this, "No key; can't encrypt query params");
    return params;
  }

  // Serialize query params to a query string that we will then
  // encrypt and place in a special query param the front-end knows is
  // encrypted.
  var queryString = this.appendParams_(params);
  
  // Nonce, really. We want 32 bits; make it so.
  var counter = this.getCount_();
  counter = counter & 0xFFFFFFFF;
  
  var encrypted = this.encryptV1(clientKeyArray, 
                                 this.VERSION,
                                 counter,
                                 queryString);

  params = {};
  params[this.VERSION_QUERY_PARAM_NAME] = this.VERSION;
  params[this.COUNT_QUERY_PARAM_NAME] = counter;
  params[this.WRAPPEDKEY_QUERY_PARAM_NAME] = wrappedKey;
  params[this.ENCRYPTED_PARAMS_PARAM_NAME] = encrypted;

  return params;
}

/**
 * Encrypts text and returns a base64 string of the results.
 *
 * This method runs in about ~2ms on a 2Ghz P4. (Turn debugging off if
 * you see it much slower).
 *
 * @param clientKeyArray Array of bytes (numbers in [0,255]) composing K_C
 *
 * @param version String indicating the version of encryption we should use.
 *
 * @param counter Number that acts as a nonce for this encryption
 *
 * @param text String to be encrypted
 *
 * @returns String containing the websafe base64-encoded ciphertext
 */
PROT_UrlCrypto.prototype.encryptV1 = function(clientKeyArray,
                                              version, 
                                              counter,
                                              text) {

  // We're a version1 encrypter, after all
  if (version != "1") 
    throw new Error("Unknown encryption version");

  var key = this.deriveEncryptionKey(clientKeyArray, counter);

  this.streamCipher_.init(key);

  if (this.RC4_DISCARD_BYTES > 0)
    this.streamCipher_.discard(this.RC4_DISCARD_BYTES);
  
  this.streamCipher_.updateFromString(text);

  var encrypted = this.streamCipher_.finish(true /* base64 encoded */);
  // The base64 version we get has new lines, we want to remove those.
  
  return encrypted.replace(/\r\n/g, "");
}
  
/**
 * Create an encryption key from K_C and a nonce
 *
 * @param clientKeyArray Array of bytes comprising K_C
 *
 * @param count Number that acts as a nonce for this key
 *
 * @return nsIKeyObject
 */
PROT_UrlCrypto.prototype.deriveEncryptionKey = function(clientKeyArray, 
                                                        count) {
  G_Assert(this, clientKeyArray instanceof Array,
           "Client key should be an array of bytes");
  G_Assert(this, typeof count == "number", "Count should be a number");
  
  // Don't clobber the client key by appending the nonce; use another array
  var paddingArray = [];
  paddingArray.push(count >> 24);
  paddingArray.push((count >> 16) & 0xFF);
  paddingArray.push((count >> 8) & 0xFF);
  paddingArray.push(count & 0xFF);

  this.hasher_.init(G_CryptoHasher.algorithms.MD5);
  this.hasher_.updateFromArray(clientKeyArray);
  this.hasher_.updateFromArray(paddingArray);

  // Create the nsIKeyObject
  var keyFactory = Cc["@mozilla.org/security/keyobjectfactory;1"]
                   .getService(Ci.nsIKeyObjectFactory);
  var key = keyFactory.keyFromString(Ci.nsIKeyObject.RC4,
                                     this.hasher_.digestRaw());
  return key;
}

/**
 * Return a new nonce for us to use. Rather than keeping a counter and
 * the headaches that entails, just use the low ms since the epoch.
 *
 * @returns 32-bit number that is the nonce to use for this encryption
 */
PROT_UrlCrypto.prototype.getCount_ = function() {
  return ((new Date).getTime() & 0xFFFFFFFF);
}

/**
 * Init the mac.  This function is called by WireFormatReader if the update
 * server has sent along a mac param.  The caller must not call initMac again
 * before calling finishMac; instead, the caller should just use another
 * UrlCrypto object.
 *
 * @param opt_clientKeyArray Optional clientKeyArray, for testing
 */
PROT_UrlCrypto.prototype.initMac = function(opt_clientKeyArray) {
  if (this.macInitialized_) {
    throw new Error("Can't interleave calls to initMac.  Please use another " +
                    "UrlCrypto object.");
  }

  this.macInitialized_ = true;

  var clientKeyArray = null;

  if (!!opt_clientKeyArray) {
    clientKeyArray = opt_clientKeyArray;
  } else {
    clientKeyArray = this.manager_.getClientKeyArray();
  }

  // Don't re-use this.hasher_, in case someone calls deriveEncryptionKey
  // between initMac and finishMac
  this.macer_.init(G_CryptoHasher.algorithms.MD5);

  this.macer_.updateFromArray(clientKeyArray);
  this.macer_.updateFromArray(this.separatorArray_);
}

/**
 * Add a line to the mac.  Called by WireFormatReader.processLine.  Not thread
 * safe.
 *
 * @param s The string to add
 */
PROT_UrlCrypto.prototype.updateMacFromString = function(s) {
  if (!this.macInitialized_) {
    throw new Error ("Initialize mac first");
  }

  var stream = Cc['@mozilla.org/io/string-input-stream;1']
               .createInstance(Ci.nsIStringInputStream);
  stream.setData(s, s.length);
  if (stream.available() > 0)
    this.macer_.updateFromStream(stream);
}

/**
 * Finish up computing the mac.  Not thread safe.
 *
 * @param opt_clientKeyArray Optional clientKeyArray, for testing
 */
PROT_UrlCrypto.prototype.finishMac = function(opt_clientKeyArray) {
  var clientKeyArray = null;
  if (!!opt_clientKeyArray) {
    clientKeyArray = opt_clientKeyArray;
  } else {
    clientKeyArray = this.manager_.getClientKeyArray();
  }

  if (!this.macInitialized_) {
    throw new Error ("Initialize mac first");
  }
  this.macer_.updateFromArray(this.separatorArray_);
  this.macer_.updateFromArray(clientKeyArray);

  this.macInitialized_ = false;

  return this.macer_.digestBase64();
}

/**
 * Compute a mac over the whole data string, and return the base64-encoded
 * string
 *
 * @param data A string
 * @param opt_outputRaw True for raw output, false for base64
 * @param opt_clientKeyArray An optional key to pass in for testing
 * @param opt_separatorArray An optional separator array to pass in for testing
 * @returns MD5(key+separator+data+separator+key)
 */
PROT_UrlCrypto.prototype.computeMac = function(data, 
                                               opt_outputRaw,
                                               opt_clientKeyArray,
                                               opt_separatorArray) {
  var clientKeyArray = null;
  var separatorArray = null;

  // Get keys and such for testing
  if (!!opt_clientKeyArray) {
    clientKeyArray = opt_clientKeyArray;
  } else {
    clientKeyArray = this.manager_.getClientKeyArray();
  }

  if (!!opt_separatorArray) {
    separatorArray = opt_separatorArray;
  } else {
    separatorArray = this.separatorArray_;
  }

  this.macer_.init(G_CryptoHasher.algorithms.MD5);

  this.macer_.updateFromArray(clientKeyArray);
  this.macer_.updateFromArray(separatorArray);

  var stream = Cc['@mozilla.org/io/string-input-stream;1']
               .createInstance(Ci.nsIStringInputStream);  
  stream.setData(data, data.length);
  if (stream.available() > 0)
    this.macer_.updateFromStream(stream);

  this.macer_.updateFromArray(separatorArray);
  this.macer_.updateFromArray(clientKeyArray);

  if (!!opt_outputRaw) {
    return this.macer_.digestRaw();
  }
  return this.macer_.digestBase64();
}

#ifdef DEBUG
/**
 * Cheeseball unittest
 */ 
function TEST_PROT_UrlCrypto() {
  if (G_GDEBUG) {
    var z = "urlcrypto UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    // We set keys on the keymanager to ensure data flows properly, so 
    // make sure we can clean up after it

    var kf = "test.txt";
    function removeTestFile(f) {
      var file = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties)
                 .get("ProfD", Ci.nsILocalFile); /* profile directory */
      file.append(f);
      if (file.exists())
        file.remove(false /* do not recurse */ );
    };
    removeTestFile(kf);

    var km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);

    // Test operation when it's not intialized

    var c = new PROT_UrlCrypto();

    var fakeManager = {
      getClientKeyArray: function() { return null; },
      getWrappedKey: function() { return null; },
    };
    c.manager_ = fakeManager;

    var params = {
      foo: "bar",
      baz: "bomb",
    };
    G_Assert(z, c.maybeCryptParams(params)["foo"] === "bar",
             "How can we encrypt if we don't have a key?");
    c.manager_ = km;
    G_Assert(z, c.maybeCryptParams(params)["foo"] === "bar",
             "Again, how can we encrypt if we don't have a key?");

    // Now we have a key! See if we can get a crypted url
    var realResponse = "clientkey:24:dtmbEN1kgN/LmuEoYifaFw==\n" +
                       "wrappedkey:24:MTpPH3pnLDKihecOci+0W5dk";
    km.onGetKeyResponse(realResponse);
    var crypted = c.maybeCryptParams(params);
    G_Assert(z, crypted["foo"] === undefined, "We have a key but can't crypt");
    G_Assert(z, crypted["bomb"] === undefined, "We have a key but can't crypt");

    // Now check to make sure all the expected query params are there
    for (var p in PROT_UrlCrypto.QPS)
      G_Assert(z, crypted[PROT_UrlCrypto.QPS[p]] != undefined, 
               "Output query params doesn't have: " + PROT_UrlCrypto.QPS[p]);
    
    // Now test that encryption is determinisitic
    
    // Some helper functions
    function arrayEquals(a1, a2) {
      if (a1.length != a2.length)
        return false;
      
      for (var i = 0; i < a1.length; i++)
        if (typeof a1[i] != typeof a2[i] || a1[i] != a2[i])
          return false;
      return true;
    };

    function arrayAsString(a) {
      var s = "[";
      for (var i = 0; i < a.length; i++)
        s += a[i] + ",";
      return s + "]";
    };

    function printArray(a) {
      var s = arrayAsString(a);
      G_Debug(z, s);
    };

    var keySizeBytes = km.clientKeyArray_.length;

    var startCrypt = (new Date).getTime();
    var numCrypts = 0;

    // Helper function to arrayify string.
    function toCharCode(c) {
      return c.charCodeAt(0);
    }
    // Set this to true for extended testing
    var doLongTest = false;
    if (doLongTest) {
      
      // Now run it through its paces. For a variety of keys of a
      // variety of lengths, and a variety of counts, encrypt
      // plaintexts of different lengths

      // For a variety of key lengths...
      for (var i = 0; i < 2 * keySizeBytes; i++) {
        var clientKeyArray = [];
        
        // For a variety of keys...
        for (var j = 0; j < keySizeBytes; j++)
          clientKeyArray[j] = i + j;
        
        // For a variety of counts...
        for (var count = 0; count < 40; count++) {
        
          var payload = "";

          // For a variety of plaintexts of different lengths
          for (var payloadPadding = 0; payloadPadding < count; payloadPadding++)
            payload += "a";
          
          var plaintext1 = Array.map(payload, toCharCode);
          var plaintext2 = Array.map(payload, toCharCode);
          var plaintext3 = Array.map(payload, toCharCode);
          
          // Verify that encryption is deterministic given set parameters
          numCrypts++;
          var ciphertext1 = c.encryptV1(clientKeyArray, 
                                      "1", 
                                        count,
                                        plaintext1);
          
          numCrypts++;        
          var ciphertext2 = c.encryptV1(clientKeyArray, 
                                        "1", 
                                        count,
                                        plaintext2);
          
          G_Assert(z, ciphertext1 === ciphertext2,
                   "Two plaintexts having different ciphertexts:" +
                   ciphertext1 + " " + ciphertext2);
          
          numCrypts++;

          // Now verify that it is symmetrical
          var b64arr = Array.map(atob(ciphertext2), toCharCode);
          var ciphertext3 = c.encryptV1(clientKeyArray, 
                                        "1", 
                                        count,
                                        b64arr,
                                        true /* websafe */);
          
          // note: ciphertext3 was websafe - reverting to plain base64 first
          var b64str = atob(ciphertext3).replace(/-/g, "+").replace(/_/g, "/");
          b64arr = Array.map(b64str, toCharCode)
          G_Assert(z, arrayEquals(plaintext3, b64arr),
                   "Encryption and decryption not symmetrical");
        }
      }
      
      // Output some interesting info
      var endCrypt = (new Date).getTime();
      var totalMS = endCrypt - startCrypt;
      G_Debug(z, "Info: Did " + numCrypts + " encryptions in " +
              totalMS + "ms, for an average of " + 
              (totalMS / numCrypts) + "ms per crypt()");
    }
      
    // Now check for compatability with C++

    var ciphertexts = {}; 
    // Generated below, and tested in C++ as well. Ciphertexts is a map
    // from substring lengths to encrypted values.

    ciphertexts[0]="";
    ciphertexts[1]="dA==";
    ciphertexts[2]="akY=";
    ciphertexts[3]="u5mV";
    ciphertexts[4]="bhtioQ==";
    ciphertexts[5]="m2wSZnQ=";
    ciphertexts[6]="zd6gWyDO";
    ciphertexts[7]="bBN0WVrlCg==";
    ciphertexts[8]="Z6U_6bMelFM=";
    ciphertexts[9]="UVoiytL-gHzp";
    ciphertexts[10]="3Xr_ZMmdmvg7zw==";
    ciphertexts[11]="PIIyif7NFRS57mY=";
    ciphertexts[12]="QEKXrRWdZ3poJVSp";
    ciphertexts[13]="T3zsAsooHuAnflNsNQ==";
    ciphertexts[14]="qgYtOJjZSIByo0KtOG0=";
    ciphertexts[15]="NsEGHGK6Ju6FjD59Byai";
    ciphertexts[16]="1RVIsC0HYoUEycoA_0UL2w==";
    ciphertexts[17]="0xXe6Lsb1tZ79T96AJTT-ps=";
    ciphertexts[18]="cVXQCYoA4RV8t1CODXuCS88y";
    ciphertexts[19]="hVf4pd4WP4wPwSyqEXRRkQZSQA==";
    ciphertexts[20]="F6Y9MHwhd1e-bDHhqNSonZbR2Sg=";
    ciphertexts[21]="TiMClYbLUdyYweW8IDytU_HD2wTM";
    ciphertexts[22]="tYQtNqz83KXE4eqn6GhAu6ZZ23SqYw==";
    ciphertexts[23]="qjL-dMpiQ2LYgkYT5IfmE1FlN36wHek=";
    ciphertexts[24]="cL7HHiOZ9PbkvZ9yrJLiv4HXcw4Nf7y7";
    ciphertexts[25]="k4I-fdR6CyzxOpR_QEG5rnvPB8IbmRnpFg==";
    ciphertexts[26]="7LjCfA1dCMjAVT_O8DpiTQ0G7igwQ1HTUMU=";
    ciphertexts[27]="CAtijc6nB-REwAkqimToMn8RC_eZAaJy9Gn4";
    ciphertexts[28]="z8sEB1lDI32wsOkgYbVZ5pxIbpCrha9BmcqxFQ==";
    ciphertexts[29]="2eysfzsfGav0vPRsSnFl8H8fg9dQCT_bSiZwno0=";
    ciphertexts[30]="2BBNlF_mtV9TB2jZHHqCAtzkJQFdVKFn7N8YxsI9";
    ciphertexts[31]="9h4-nldHAr77Boks7lPzsi8TwVCIQzSkiJp2xatbGg==";
    ciphertexts[32]="DHTB8bDTXpUIrZ2ZlAujXLi-501NoWUVIEQJLaKCpqQ=";
    ciphertexts[33]="E9Av2GgnZg_q5r-JLSzM_ShCu1yPF2VeCaQfPPXSSE4I";
    ciphertexts[34]="UJzEucVBnGEfRNBQ6tvbaro0_I_-mQeJMpU2zQnfFdBuFg==";
    ciphertexts[35]="_p0OYras-Vn2rQ9X-J0dFRnhCfytuTEjheUTU7Ueaf1rIA4=";
    ciphertexts[36]="Q0nZXFPJbpx1WZPP-lLPuSGR-pD08B4CAW-6Uf0eEkS05-oM";
    ciphertexts[37]="XeKfieZGc9bPh7nRtCgujF8OY14zbIZSK20Lwg1HTpHi9HfXVQ==";
    
    var clientKey = "dtmbEN1kgN/LmuEoYifaFw==";
    var clientKeyArray = Array.map(atob(clientKey), toCharCode);
    // wrappedKey was "MTpPH3pnLDKihecOci+0W5dk"
    var count = 0xFEDCBA09;
    var plaintext = "http://www.foobar.com/this?is&some=url";
    
    // For every substring of the plaintext, change the count and verify
    // that we get what we expect when we encrypt

    for (var i = 0; i < plaintext.length; i++) {
      var plaintextArray = Array.map(plaintext.slice(0, i), toCharCode);
      var crypted = c.encryptV1(clientKeyArray,
                                "1",
                                count + i,
                                plaintextArray);
      G_Assert(z, crypted === ciphertexts[i], 
               "Generated unexpected ciphertext");

      // Uncomment to generate
      // dump("\nciphertexts[" + i + "]=\"" + crypted + "\";");
    }

    // Cribbed from the MD5 rfc to make sure we're computing plain'ol md5
    // values correctly
    // http://www.faqs.org/rfcs/rfc1321.html
    var md5texts = [ "",
                     "a", 
                     "abc", 
                     "message digest",
                     "abcdefghijklmnopqrstuvwxyz",
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                     "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
		   ];

    var md5digests = [ "d41d8cd98f00b204e9800998ecf8427e", 
                       "0cc175b9c0f1b6a831c399e269772661", 
                       "900150983cd24fb0d6963f7d28e17f72", 
                       "f96b697d7cb7938d525a2f31aaf161d0", 
                       "c3fcd3d76192e4007dfb496cca67e13b", 
                       "d174ab98d277d9f5a5611c2c9f419d9f", 
                       "57edf4a22be3c955ac49da2e2107b67a"
		     ];

    var expected_mac = [];
    expected_mac[0] = "ZOJ6Mk+ccC6R7BwseqCYQQ==";
    expected_mac[1] = "zWM7tvcsuH/MSEviNiRbOA==";
    expected_mac[2] = "ZAUVyls/6ZVN3Np8v3pX3g==";
    expected_mac[3] = "Zq6gF7RkPwKqlicuxrO4mg==";
    expected_mac[4] = "/LOJETSnqSW3q4u1hs/0Pg==";
    expected_mac[5] = "jjOEX7H2uchOznxIGuqzJg==";
    expected_mac[6] = "Tje7aP/Rk/gkSH4he0KMQQ==";

    // Check plain'ol MD5
    var hasher = new G_CryptoHasher();
    for (var i = 0; i < md5texts.length; i++) {
      var computedMac = c.computeMac(md5texts[i], true /* output raw */, [], []);
      var hex = hasher.toHex_(computedMac).toLowerCase();
      G_Assert(z, hex == md5digests[i], 
               "MD5(" + md5texts[i] + ") = " + md5digests[i] + ", not " + hex);
    }

    for (var i = 0; i < md5texts.length; i++) {
      var computedMac = c.computeMac(md5texts[i], 
                                  false /* output Base64 */,
                                  clientKeyArray);
      G_Assert(z, computedMac == expected_mac[i], 
               "Wrong mac generated for " + md5texts[i]);
      // Uncomment to generate
      // dump("\nexpected_mac[" + i + "] = \"" + computedMac + "\";");
    }

    // Now check if adding line by line is the same as computing over the whole
    // thing
    var wholeString = md5texts[0] + md5texts[1];
    var wholeStringMac = c.computeMac(wholeString,
                                      false /* output Base64 */,
				      clientKeyArray);

    expected_mac = "zWM7tvcsuH/MSEviNiRbOA==";
    c.initMac(clientKeyArray);
    c.updateMacFromString(md5texts[0]);
    c.updateMacFromString(md5texts[1]);
    var piecemealMac = c.finishMac(clientKeyArray);
    G_Assert(z, piecemealMac == wholeStringMac,
             "Computed different values for mac when adding line by line!");
    G_Assert(z, piecemealMac == expected_mac, "Didn't generate expected mac");

    // Tested also in cheesey-test-frontend.sh and urlcrypto_unittest.cc on
    // the server side
    expected_mac = "iA5vLUidpXAPwfcAH9+8OQ==";
    var set3data = "";
    for (var i = 1; i <= 3; i++) {
      set3data += "+white" + i + ".com\t1\n";
    }
    var computedMac = c.computeMac(set3data, false /*Base64*/, clientKeyArray);
    G_Assert(z, expected_mac == computedMac, "Expected " + expected_mac, " got " + computedMac);

    removeTestFile(kf);

    G_Debug(z, "PASS");
  }
}
#endif
