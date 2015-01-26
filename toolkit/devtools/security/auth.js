/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Services = require("Services");
loader.lazyRequireGetter(this, "prompt",
  "devtools/toolkit/security/prompt");

/**
 * An |Authenticator| implements an authentication mechanism via various hooks
 * in the client and server debugger socket connection path (see socket.js).
 *
 * |Authenticator|s are stateless objects.  Each hook method is passed the state
 * it needs by the client / server code in socket.js.
 *
 * Separate instances of the |Authenticator| are created for each use (client
 * connection, server listener) in case some methods are customized by the
 * embedder for a given use case.
 */
let Authenticators = {};

/**
 * The Prompt authenticator displays a server-side user prompt that includes
 * connection details, and asks the user to verify the connection.  There are
 * no cryptographic properties at work here, so it is up to the user to be sure
 * that the client can be trusted.
 */
let Prompt = Authenticators.Prompt = {};

Prompt.mode = "PROMPT";

Prompt.Client = function() {};
Prompt.Client.prototype = {

  mode: Prompt.mode,

};

Prompt.Server = function() {};
Prompt.Server.prototype = {

  mode: Prompt.mode,

  /**
   * Verify that listener settings are appropriate for this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener about to be opened.
   * @throws if validation requirements are not met
   */
  validateOptions() {},

  /**
   * Augment the service discovery advertisement with any additional data needed
   * to support this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener that was just opened.
   * @param advertisement object
   *        The advertisement being built.
   */
  augmentAdvertisement(listener, advertisement) {
    advertisement.authentication = Prompt.mode;
  },

  /**
   * Determine whether a connection the server should be allowed or not based on
   * this authenticator's policies.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  authenticate() {
    return !Services.prefs.getBoolPref("devtools.debugger.prompt-connection") ||
           this.allowConnection();
  },

  /**
   * Prompt the user to accept or decline the incoming connection. The default
   * implementation is used unless this is overridden on a particular
   * authenticator instance.
   *
   * In PROMPT mode, the |allowConnection| method is provided:
   * {
   *   authentication: "PROMPT",
   *   client: {
   *     host,
   *     port
   *   },
   *   server: {
   *     host,
   *     port
   *   }
   * }
   *
   * It is expected that the implementation of |allowConnection| will show a
   * prompt to the user so that they can allow or deny the connection.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  allowConnection: prompt.Server.defaultAllowConnection,

};

/**
 * The out-of-band (OOB) cert authenticator is based on self-signed X.509 certs
 * at both the client and server end.
 *
 * The user is first prompted to verify the connection, similar to the prompt
 * method above.  This prompt may display cert fingerprints if desired.
 *
 * Assuming the user approves the connection, further UI is used to assist the
 * user in tranferring out-of-band (OOB) verification of the client's
 * certificate.  For example, this could take the form of a QR code that the
 * client displays which is then scanned by a camera on the server.
 *
 * Since it is assumed that an attacker can't forge the client's X.509 cert, the
 * user may also choose to always allow a client, which would permit immediate
 * connections in the future with no user interaction needed.
 *
 * See docs/wifi.md for details of the authentication design.
 */
let OOBCert = Authenticators.OOBCert = {};

OOBCert.mode = "OOB_CERT";

OOBCert.Client = function() {};
OOBCert.Client.prototype = {

  mode: OOBCert.mode,

};

OOBCert.Server = function() {};
OOBCert.Server.prototype = {

  mode: OOBCert.mode,

  /**
   * Verify that listener settings are appropriate for this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener about to be opened.
   * @throws if validation requirements are not met
   */
  validateOptions(listener) {
    if (!listener.encryption) {
      throw new Error(OOBCert.mode + " authentication requires encryption.");
    }
  },

  /**
   * Augment the service discovery advertisement with any additional data needed
   * to support this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener that was just opened.
   * @param advertisement object
   *        The advertisement being built.
   */
  augmentAdvertisement(listener, advertisement) {
    advertisement.authentication = OOBCert.mode;
    // Step A.4
    // Server announces itself via service discovery
    // Announcement contains hash(ServerCert) as additional data
    advertisement.cert = {
      sha256: listener._socket.serverCert.sha256Fingerprint
    };
  },

  /**
   * Determine whether a connection the server should be allowed or not based on
   * this authenticator's policies.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  authenticate() {
    return this.allowConnection();
  },

  /**
   * Prompt the user to accept or decline the incoming connection. The default
   * implementation is used unless this is overridden on a particular
   * authenticator instance.
   *
   * In OOB_CERT mode, the |allowConnection| method is provided:
   * {
   *   authentication: "OOB_CERT",
   *   client: {
   *     host,
   *     port,
   *     cert: {
   *       sha256
   *     },
   *   },
   *   server: {
   *     host,
   *     port,
   *     cert: {
   *       sha256
   *     }
   *   }
   * }
   *
   * It is expected that the implementation of |allowConnection| will show a
   * prompt to the user so that they can allow or deny the connection.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  allowConnection: prompt.Server.defaultAllowConnection,

};

exports.Authenticators = {
  get(mode) {
    if (!mode) {
      mode = Prompt.mode;
    }
    for (let key in Authenticators) {
      let auth = Authenticators[key];
      if (auth.mode === mode) {
        return auth;
      }
    }
    throw new Error("Unknown authenticator mode: " + mode);
  }
};
