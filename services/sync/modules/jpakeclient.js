/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["JPAKEClient", "SendCredentialsController"];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/util.js");

const REQUEST_TIMEOUT         = 60; // 1 minute
const KEYEXCHANGE_VERSION     = 3;

const JPAKE_SIGNERID_SENDER   = "sender";
const JPAKE_SIGNERID_RECEIVER = "receiver";
const JPAKE_LENGTH_SECRET     = 8;
const JPAKE_LENGTH_CLIENTID   = 256;
const JPAKE_VERIFY_VALUE      = "0123456789ABCDEF";


/**
 * Client to exchange encrypted data using the J-PAKE algorithm.
 * The exchange between two clients of this type looks like this:
 * 
 * 
 *  Mobile                        Server                        Desktop
 *  ===================================================================
 *                                   |
 *  retrieve channel <---------------|
 *  generate random secret           |
 *  show PIN = secret + channel      |                 ask user for PIN
 *  upload Mobile's message 1 ------>|
 *                                   |----> retrieve Mobile's message 1
 *                                   |<----- upload Desktop's message 1
 *  retrieve Desktop's message 1 <---|
 *  upload Mobile's message 2 ------>|
 *                                   |----> retrieve Mobile's message 2
 *                                   |                      compute key
 *                                   |<----- upload Desktop's message 2
 *  retrieve Desktop's message 2 <---|
 *  compute key                      |
 *  encrypt known value ------------>|
 *                                   |-------> retrieve encrypted value
 *                                   | verify against local known value
 *
 *   At this point Desktop knows whether the PIN was entered correctly.
 *   If it wasn't, Desktop deletes the session. If it was, the account
 *   setup can proceed. If Desktop doesn't yet have an account set up,
 *   it will keep the channel open and let the user connect to or
 *   create an account.
 *
 *                                   |              encrypt credentials
 *                                   |<------------- upload credentials
 *  retrieve credentials <-----------|
 *  verify HMAC                      |
 *  decrypt credentials              |
 *  delete session ----------------->|
 *  start syncing                    |
 * 
 * 
 * Create a client object like so:
 * 
 *   let client = new JPAKEClient(controller);
 * 
 * The 'controller' object must implement the following methods:
 * 
 *   displayPIN(pin) -- Called when a PIN has been generated and is ready to
 *     be displayed to the user. Only called on the client where the pairing
 *     was initiated with 'receiveNoPIN()'.
 * 
 *   onPairingStart() -- Called when the pairing has started and messages are
 *     being sent back and forth over the channel. Only called on the client
 *     where the pairing was initiated with 'receiveNoPIN()'.
 * 
 *   onPaired() -- Called when the device pairing has been established and
 *     we're ready to send the credentials over. To do that, the controller
 *     must call 'sendAndComplete()' while the channel is active.
 * 
 *   onComplete(data) -- Called after transfer has been completed. On
 *     the sending side this is called with no parameter and as soon as the
 *     data has been uploaded. This does not mean the receiving side has
 *     actually retrieved them yet.
 *
 *   onAbort(error) -- Called whenever an error is encountered. All errors lead
 *     to an abort and the process has to be started again on both sides.
 * 
 * To start the data transfer on the receiving side, call
 * 
 *   client.receiveNoPIN();
 * 
 * This will allocate a new channel on the server, generate a PIN, have it
 * displayed and then do the transfer once the protocol has been completed
 * with the sending side.
 * 
 * To initiate the transfer from the sending side, call
 * 
 *   client.pairWithPIN(pin, true);
 * 
 * Once the pairing has been established, the controller's 'onPaired()' method
 * will be called. To then transmit the data, call
 * 
 *   client.sendAndComplete(data);
 * 
 * To abort the process, call
 * 
 *   client.abort();
 * 
 * Note that after completion or abort, the 'client' instance may not be reused.
 * You will have to create a new one in case you'd like to restart the process.
 */
this.JPAKEClient = function JPAKEClient(controller) {
  this.controller = controller;

  this._log = Log4Moz.repository.getLogger("Sync.JPAKEClient");
  this._log.level = Log4Moz.Level[Svc.Prefs.get(
    "log.logger.service.jpakeclient", "Debug")];

  this._serverURL = Svc.Prefs.get("jpake.serverURL");
  this._pollInterval = Svc.Prefs.get("jpake.pollInterval");
  this._maxTries = Svc.Prefs.get("jpake.maxTries");
  if (this._serverURL.slice(-1) != "/") {
    this._serverURL += "/";
  }

  this._jpake = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
                  .createInstance(Ci.nsISyncJPAKE);

  this._setClientID();
}
JPAKEClient.prototype = {

  _chain: Async.chain,

  /*
   * Public API
   */

  /**
   * Initiate pairing and receive data without providing a PIN. The PIN will
   * be generated and passed on to the controller to be displayed to the user.
   * 
   * This is typically called on mobile devices where typing is tedious.
   */
  receiveNoPIN: function receiveNoPIN() {
    this._my_signerid = JPAKE_SIGNERID_RECEIVER;
    this._their_signerid = JPAKE_SIGNERID_SENDER;

    this._secret = this._createSecret();

    // Allow a large number of tries first while we wait for the PIN
    // to be entered on the other device.
    this._maxTries = Svc.Prefs.get("jpake.firstMsgMaxTries");
    this._chain(this._getChannel,
                this._computeStepOne,
                this._putStep,
                this._getStep,
                function(callback) {
                  // We fetched the first response from the other client.
                  // Notify controller of the pairing starting.
                  Utils.nextTick(this.controller.onPairingStart,
                                 this.controller);

                  // Now we can switch back to the smaller timeout.
                  this._maxTries = Svc.Prefs.get("jpake.maxTries");
                  callback();
                },
                this._computeStepTwo,
                this._putStep,
                this._getStep,
                this._computeFinal,
                this._computeKeyVerification,
                this._putStep,
                function(callback) {
                  // Allow longer time-out for the last message.
                  this._maxTries = Svc.Prefs.get("jpake.lastMsgMaxTries");
                  callback();
                },
                this._getStep,
                this._decryptData,
                this._complete)();
  },

  /**
   * Initiate pairing based on the PIN entered by the user.
   * 
   * This is typically called on desktop devices where typing is easier than
   * on mobile.
   * 
   * @param pin
   *        12 character string (in human-friendly base32) containing the PIN
   *        entered by the user.
   * @param expectDelay
   *        Flag that indicates that a significant delay between the pairing
   *        and the sending should be expected. v2 and earlier of the protocol
   *        did not allow for this and the pairing to a v2 or earlier client
   *        will be aborted if this flag is 'true'.
   */
  pairWithPIN: function pairWithPIN(pin, expectDelay) {
    this._my_signerid = JPAKE_SIGNERID_SENDER;
    this._their_signerid = JPAKE_SIGNERID_RECEIVER;

    this._channel = pin.slice(JPAKE_LENGTH_SECRET);
    this._channelURL = this._serverURL + this._channel;
    this._secret = pin.slice(0, JPAKE_LENGTH_SECRET);

    this._chain(this._computeStepOne,
                this._getStep,
                function (callback) {
                  // Ensure that the other client can deal with a delay for
                  // the last message if that's requested by the caller.
                  if (!expectDelay) {
                    return callback();
                  }
                  if (!this._incoming.version || this._incoming.version < 3) {
                    return this.abort(JPAKE_ERROR_DELAYUNSUPPORTED);
                  }
                  return callback();
                },
                this._putStep,
                this._computeStepTwo,
                this._getStep,
                this._putStep,
                this._computeFinal,
                this._getStep,
                this._verifyPairing)();
  },

  /**
   * Send data after a successful pairing.
   * 
   * @param obj
   *        Object containing the data to send. It will be serialized as JSON.
   */
  sendAndComplete: function sendAndComplete(obj) {
    if (!this._paired || this._finished) {
      this._log.error("Can't send data, no active pairing!");
      throw "No active pairing!";
    }
    this._data = JSON.stringify(obj);
    this._chain(this._encryptData,
                this._putStep,
                this._complete)();
  },

  /**
   * Abort the current pairing. The channel on the server will be deleted
   * if the abort wasn't due to a network or server error. The controller's
   * 'onAbort()' method is notified in all cases.
   * 
   * @param error [optional]
   *        Error constant indicating the reason for the abort. Defaults to
   *        user abort.
   */
  abort: function abort(error) {
    this._log.debug("Aborting...");
    this._finished = true;
    let self = this;

    // Default to "user aborted".
    if (!error) {
      error = JPAKE_ERROR_USERABORT;
    }

    if (error == JPAKE_ERROR_CHANNEL ||
        error == JPAKE_ERROR_NETWORK ||
        error == JPAKE_ERROR_NODATA) {
      Utils.nextTick(function() { this.controller.onAbort(error); }, this);
    } else {
      this._reportFailure(error, function() { self.controller.onAbort(error); });
    }
  },

  /*
   * Utilities
   */

  _setClientID: function _setClientID() {
    let rng = Cc["@mozilla.org/security/random-generator;1"]
                .createInstance(Ci.nsIRandomGenerator);
    let bytes = rng.generateRandomBytes(JPAKE_LENGTH_CLIENTID / 2);
    this._clientID = [("0" + byte.toString(16)).slice(-2)
                      for each (byte in bytes)].join("");
  },

  _createSecret: function _createSecret() {
    // 0-9a-z without 1,l,o,0
    const key = "23456789abcdefghijkmnpqrstuvwxyz";
    let rng = Cc["@mozilla.org/security/random-generator;1"]
                .createInstance(Ci.nsIRandomGenerator);
    let bytes = rng.generateRandomBytes(JPAKE_LENGTH_SECRET);
    return [key[Math.floor(byte * key.length / 256)]
            for each (byte in bytes)].join("");
  },

  _newRequest: function _newRequest(uri) {
    let request = new RESTRequest(uri);
    request.setHeader("X-KeyExchange-Id", this._clientID);
    request.timeout = REQUEST_TIMEOUT;
    return request;
  },

  /*
   * Steps of J-PAKE procedure
   */

  _getChannel: function _getChannel(callback) {
    this._log.trace("Requesting channel.");
    let request = this._newRequest(this._serverURL + "new_channel");
    request.get(Utils.bind2(this, function handleChannel(error) {
      if (this._finished) {
        return;
      }

      if (error) {
        this._log.error("Error acquiring channel ID. " + error);
        this.abort(JPAKE_ERROR_CHANNEL);
        return;
      }
      if (request.response.status != 200) {
        this._log.error("Error acquiring channel ID. Server responded with HTTP "
                        + request.response.status);
        this.abort(JPAKE_ERROR_CHANNEL);
        return;
      }

      try {
        this._channel = JSON.parse(request.response.body);
      } catch (ex) {
        this._log.error("Server responded with invalid JSON.");
        this.abort(JPAKE_ERROR_CHANNEL);
        return;
      }
      this._log.debug("Using channel " + this._channel);
      this._channelURL = this._serverURL + this._channel;

      // Don't block on UI code.
      let pin = this._secret + this._channel;
      Utils.nextTick(function() { this.controller.displayPIN(pin); }, this);
      callback();
    }));
  },

  // Generic handler for uploading data.
  _putStep: function _putStep(callback) {
    this._log.trace("Uploading message " + this._outgoing.type);
    let request = this._newRequest(this._channelURL);
    if (this._their_etag) {
      request.setHeader("If-Match", this._their_etag);
    } else {
      request.setHeader("If-None-Match", "*");
    }
    request.put(this._outgoing, Utils.bind2(this, function (error) {
      if (this._finished) {
        return;
      }

      if (error) {
        this._log.error("Error uploading data. " + error);
        this.abort(JPAKE_ERROR_NETWORK);
        return;
      }
      if (request.response.status != 200) {
        this._log.error("Could not upload data. Server responded with HTTP "
                        + request.response.status);
        this.abort(JPAKE_ERROR_SERVER);
        return;
      }
      // There's no point in returning early here since the next step will
      // always be a GET so let's pause for twice the poll interval.
      this._my_etag = request.response.headers["etag"];
      Utils.namedTimer(function () { callback(); }, this._pollInterval * 2,
                       this, "_pollTimer");
    }));
  },

  // Generic handler for polling for and retrieving data.
  _pollTries: 0,
  _getStep: function _getStep(callback) {
    this._log.trace("Retrieving next message.");
    let request = this._newRequest(this._channelURL);
    if (this._my_etag) {
      request.setHeader("If-None-Match", this._my_etag);
    }

    request.get(Utils.bind2(this, function (error) {
      if (this._finished) {
        return;
      }

      if (error) {
        this._log.error("Error fetching data. " + error);
        this.abort(JPAKE_ERROR_NETWORK);
        return;
      }

      if (request.response.status == 304) {
        this._log.trace("Channel hasn't been updated yet. Will try again later.");
        if (this._pollTries >= this._maxTries) {
          this._log.error("Tried for " + this._pollTries + " times, aborting.");
          this.abort(JPAKE_ERROR_TIMEOUT);
          return;
        }
        this._pollTries += 1;
        Utils.namedTimer(function() { this._getStep(callback); },
                         this._pollInterval, this, "_pollTimer");
        return;
      }
      this._pollTries = 0;

      if (request.response.status == 404) {
        this._log.error("No data found in the channel.");
        this.abort(JPAKE_ERROR_NODATA);
        return;
      }
      if (request.response.status != 200) {
        this._log.error("Could not retrieve data. Server responded with HTTP "
                        + request.response.status);
        this.abort(JPAKE_ERROR_SERVER);
        return;
      }

      this._their_etag = request.response.headers["etag"];
      if (!this._their_etag) {
        this._log.error("Server did not supply ETag for message: "
                        + request.response.body);
        this.abort(JPAKE_ERROR_SERVER);
        return;
      }

      try {
        this._incoming = JSON.parse(request.response.body);
      } catch (ex) {
        this._log.error("Server responded with invalid JSON.");
        this.abort(JPAKE_ERROR_INVALID);
        return;
      }
      this._log.trace("Fetched message " + this._incoming.type);
      callback();
    }));
  },

  _reportFailure: function _reportFailure(reason, callback) {
    this._log.debug("Reporting failure to server.");
    let request = this._newRequest(this._serverURL + "report");
    request.setHeader("X-KeyExchange-Cid", this._channel);
    request.setHeader("X-KeyExchange-Log", reason);
    request.post("", Utils.bind2(this, function (error) {
      if (error) {
        this._log.warn("Report failed: " + error);
      } else if (request.response.status != 200) {
        this._log.warn("Report failed. Server responded with HTTP "
                       + request.response.status);
      }

      // Do not block on errors, we're done or aborted by now anyway.
      callback();
    }));
  },

  _computeStepOne: function _computeStepOne(callback) {
    this._log.trace("Computing round 1.");
    let gx1 = {};
    let gv1 = {};
    let r1 = {};
    let gx2 = {};
    let gv2 = {};
    let r2 = {};
    try {
      this._jpake.round1(this._my_signerid, gx1, gv1, r1, gx2, gv2, r2);
    } catch (ex) {
      this._log.error("JPAKE round 1 threw: " + ex);
      this.abort(JPAKE_ERROR_INTERNAL);
      return;
    }
    let one = {gx1: gx1.value,
               gx2: gx2.value,
               zkp_x1: {gr: gv1.value, b: r1.value, id: this._my_signerid},
               zkp_x2: {gr: gv2.value, b: r2.value, id: this._my_signerid}};
    this._outgoing = {type: this._my_signerid + "1",
                      version: KEYEXCHANGE_VERSION,
                      payload: one};
    this._log.trace("Generated message " + this._outgoing.type);
    callback();
  },

  _computeStepTwo: function _computeStepTwo(callback) {
    this._log.trace("Computing round 2.");
    if (this._incoming.type != this._their_signerid + "1") {
      this._log.error("Invalid round 1 message: "
                      + JSON.stringify(this._incoming));
      this.abort(JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    let step1 = this._incoming.payload;
    if (!step1 || !step1.zkp_x1 || step1.zkp_x1.id != this._their_signerid
        || !step1.zkp_x2 || step1.zkp_x2.id != this._their_signerid) {
      this._log.error("Invalid round 1 payload: " + JSON.stringify(step1));
      this.abort(JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    let A = {};
    let gvA = {};
    let rA = {};

    try {
      this._jpake.round2(this._their_signerid, this._secret,
                         step1.gx1, step1.zkp_x1.gr, step1.zkp_x1.b,
                         step1.gx2, step1.zkp_x2.gr, step1.zkp_x2.b,
                         A, gvA, rA);
    } catch (ex) {
      this._log.error("JPAKE round 2 threw: " + ex);
      this.abort(JPAKE_ERROR_INTERNAL);
      return;
    }
    let two = {A: A.value,
               zkp_A: {gr: gvA.value, b: rA.value, id: this._my_signerid}};
    this._outgoing = {type: this._my_signerid + "2",
                      version: KEYEXCHANGE_VERSION,
                      payload: two};
    this._log.trace("Generated message " + this._outgoing.type);
    callback();
  },

  _computeFinal: function _computeFinal(callback) {
    if (this._incoming.type != this._their_signerid + "2") {
      this._log.error("Invalid round 2 message: "
                      + JSON.stringify(this._incoming));
      this.abort(JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    let step2 = this._incoming.payload;
    if (!step2 || !step2.zkp_A || step2.zkp_A.id != this._their_signerid) {
      this._log.error("Invalid round 2 payload: " + JSON.stringify(step1));
      this.abort(JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    let aes256Key = {};
    let hmac256Key = {};

    try {
      this._jpake.final(step2.A, step2.zkp_A.gr, step2.zkp_A.b, HMAC_INPUT,
                        aes256Key, hmac256Key);
    } catch (ex) {
      this._log.error("JPAKE final round threw: " + ex);
      this.abort(JPAKE_ERROR_INTERNAL);
      return;
    }

    this._crypto_key = aes256Key.value;
    let hmac_key = Utils.makeHMACKey(Utils.safeAtoB(hmac256Key.value));
    this._hmac_hasher = Utils.makeHMACHasher(Ci.nsICryptoHMAC.SHA256, hmac_key);

    callback();
  },

  _computeKeyVerification: function _computeKeyVerification(callback) {
    this._log.trace("Encrypting key verification value.");
    let iv, ciphertext;
    try {
      iv = Svc.Crypto.generateRandomIV();
      ciphertext = Svc.Crypto.encrypt(JPAKE_VERIFY_VALUE,
                                      this._crypto_key, iv);
    } catch (ex) {
      this._log.error("Failed to encrypt key verification value.");
      this.abort(JPAKE_ERROR_INTERNAL);
      return;
    }
    this._outgoing = {type: this._my_signerid + "3",
                      version: KEYEXCHANGE_VERSION,
                      payload: {ciphertext: ciphertext, IV: iv}};
    this._log.trace("Generated message " + this._outgoing.type);
    callback();
  },

  _verifyPairing: function _verifyPairing(callback) {
    this._log.trace("Verifying their key.");
    if (this._incoming.type != this._their_signerid + "3") {
      this._log.error("Invalid round 3 data: " +
                      JSON.stringify(this._incoming));
      this.abort(JPAKE_ERROR_WRONGMESSAGE);
      return;
    }
    let step3 = this._incoming.payload;
    let ciphertext;
    try {
      ciphertext = Svc.Crypto.encrypt(JPAKE_VERIFY_VALUE,
                                      this._crypto_key, step3.IV);
      if (ciphertext != step3.ciphertext) {
        throw "Key mismatch!";
      }
    } catch (ex) {
      this._log.error("Keys don't match!");
      this.abort(JPAKE_ERROR_KEYMISMATCH);
      return;
    }

    this._log.debug("Verified pairing!");
    this._paired = true;
    Utils.nextTick(function () { this.controller.onPaired(); }, this);
    callback();
  },

  _encryptData: function _encryptData(callback) {
    this._log.trace("Encrypting data.");
    let iv, ciphertext, hmac;
    try {
      iv = Svc.Crypto.generateRandomIV();
      ciphertext = Svc.Crypto.encrypt(this._data, this._crypto_key, iv);
      hmac = Utils.bytesAsHex(Utils.digestUTF8(ciphertext, this._hmac_hasher));
    } catch (ex) {
      this._log.error("Failed to encrypt data.");
      this.abort(JPAKE_ERROR_INTERNAL);
      return;
    }
    this._outgoing = {type: this._my_signerid + "3",
                      version: KEYEXCHANGE_VERSION,
                      payload: {ciphertext: ciphertext, IV: iv, hmac: hmac}};
    this._log.trace("Generated message " + this._outgoing.type);
    callback();
  },

  _decryptData: function _decryptData(callback) {
    this._log.trace("Verifying their key.");
    if (this._incoming.type != this._their_signerid + "3") {
      this._log.error("Invalid round 3 data: "
                      + JSON.stringify(this._incoming));
      this.abort(JPAKE_ERROR_WRONGMESSAGE);
      return;
    }
    let step3 = this._incoming.payload;
    try {
      let hmac = Utils.bytesAsHex(
        Utils.digestUTF8(step3.ciphertext, this._hmac_hasher));
      if (hmac != step3.hmac) {
        throw "HMAC validation failed!";
      }
    } catch (ex) {
      this._log.error("HMAC validation failed.");
      this.abort(JPAKE_ERROR_KEYMISMATCH);
      return;
    }

    this._log.trace("Decrypting data.");
    let cleartext;
    try {      
      cleartext = Svc.Crypto.decrypt(step3.ciphertext, this._crypto_key,
                                     step3.IV);
    } catch (ex) {
      this._log.error("Failed to decrypt data.");
      this.abort(JPAKE_ERROR_INTERNAL);
      return;
    }

    try {
      this._newData = JSON.parse(cleartext);
    } catch (ex) {
      this._log.error("Invalid data data: " + JSON.stringify(cleartext));
      this.abort(JPAKE_ERROR_INVALID);
      return;
    }

    this._log.trace("Decrypted data.");
    callback();
  },

  _complete: function _complete() {
    this._log.debug("Exchange completed.");
    this._finished = true;
    Utils.nextTick(function () { this.controller.onComplete(this._newData); },
                   this);
  }

};


/**
 * Send credentials over an active J-PAKE channel.
 *
 * This object is designed to take over as the JPAKEClient controller,
 * presumably replacing one that is UI-based which would either cause
 * DOM objects to leak or the JPAKEClient to be GC'ed when the DOM
 * context disappears. This object stays alive for the duration of the
 * transfer by being strong-ref'ed as an nsIObserver.
 *
 * Credentials are sent after the first sync has been completed
 * (successfully or not.)
 *
 * Usage:
 *
 *   jpakeclient.controller = new SendCredentialsController(jpakeclient,
 *                                                          service);
 *
 */
this.SendCredentialsController =
 function SendCredentialsController(jpakeclient, service) {
  this._log = Log4Moz.repository.getLogger("Sync.SendCredentialsController");
  this._log.level = Log4Moz.Level[Svc.Prefs.get("log.logger.service.main")];

  this._log.trace("Loading.");
  this.jpakeclient = jpakeclient;
  this.service = service;

  // Register ourselves as observers the first Sync finishing (either
  // successfully or unsuccessfully, we don't care) or for removing
  // this device's sync configuration, in case that happens while we
  // haven't finished the first sync yet.
  Services.obs.addObserver(this, "weave:service:sync:finish", false);
  Services.obs.addObserver(this, "weave:service:sync:error",  false);
  Services.obs.addObserver(this, "weave:service:start-over",  false);
}
SendCredentialsController.prototype = {

  unload: function unload() {
    this._log.trace("Unloading.");
    try {
      Services.obs.removeObserver(this, "weave:service:sync:finish");
      Services.obs.removeObserver(this, "weave:service:sync:error");
      Services.obs.removeObserver(this, "weave:service:start-over");
    } catch (ex) {
      // Ignore.
    }
  },

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:service:sync:finish":
      case "weave:service:sync:error":
        Utils.nextTick(this.sendCredentials, this);
        break;
      case "weave:service:start-over":
        // This will call onAbort which will call unload().
        this.jpakeclient.abort();
        break;
    }
  },

  sendCredentials: function sendCredentials() {
    this._log.trace("Sending credentials.");
    let credentials = {account:   this.service.identity.account,
                       password:  this.service.identity.basicPassword,
                       synckey:   this.service.identity.syncKey,
                       serverURL: this.service.serverURL};
    this.jpakeclient.sendAndComplete(credentials);
  },

  // JPAKEClient controller API

  onComplete: function onComplete() {
    this._log.debug("Exchange was completed successfully!");
    this.unload();

    // Schedule a Sync for soonish to fetch the data uploaded by the
    // device with which we just paired.
    this.service.scheduler.scheduleNextSync(this.service.scheduler.activeInterval);
  },

  onAbort: function onAbort(error) {
    // It doesn't really matter why we aborted, but the channel is closed
    // for sure, so we won't be able to do anything with it.
    this._log.debug("Exchange was aborted with error: " + error);
    this.unload();
  },

  // Irrelevant methods for this controller:
  displayPIN: function displayPIN() {},
  onPairingStart: function onPairingStart() {},
  onPaired: function onPaired() {},
};
