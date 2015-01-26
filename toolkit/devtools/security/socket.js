/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Ci, Cc, CC, Cr } = require("chrome");

// Ensure PSM is initialized to support TLS sockets
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

let Services = require("Services");
let promise = require("promise");
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let { dumpn, dumpv } = DevToolsUtils;
loader.lazyRequireGetter(this, "DebuggerTransport",
  "devtools/toolkit/transport/transport", true);
loader.lazyRequireGetter(this, "DebuggerServer",
  "devtools/server/main", true);
loader.lazyRequireGetter(this, "discovery",
  "devtools/toolkit/discovery/discovery");
loader.lazyRequireGetter(this, "cert",
  "devtools/toolkit/security/cert");
loader.lazyRequireGetter(this, "Authenticators",
  "devtools/toolkit/security/auth", true);
loader.lazyRequireGetter(this, "prompt",
  "devtools/toolkit/security/prompt");
loader.lazyRequireGetter(this, "setTimeout", "Timer", true);
loader.lazyRequireGetter(this, "clearTimeout", "Timer", true);

DevToolsUtils.defineLazyGetter(this, "nsFile", () => {
  return CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
});

DevToolsUtils.defineLazyGetter(this, "socketTransportService", () => {
  return Cc["@mozilla.org/network/socket-transport-service;1"]
         .getService(Ci.nsISocketTransportService);
});

DevToolsUtils.defineLazyGetter(this, "certOverrideService", () => {
  return Cc["@mozilla.org/security/certoverride;1"]
         .getService(Ci.nsICertOverrideService);
});

DevToolsUtils.defineLazyGetter(this, "nssErrorsService", () => {
  return Cc["@mozilla.org/nss_errors_service;1"]
         .getService(Ci.nsINSSErrorsService);
});

DevToolsUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

let DebuggerSocket = {};

/**
 * Connects to a debugger server socket.
 *
 * @param host string
 *        The host name or IP address of the debugger server.
 * @param port number
 *        The port number of the debugger server.
 * @param encryption boolean (optional)
 *        Whether the server requires encryption.  Defaults to false.
 * @return promise
 *         Resolved to a DebuggerTransport instance.
 */
DebuggerSocket.connect = Task.async(function*({ host, port, encryption }) {
  let attempt = yield _attemptTransport({ host, port, encryption });
  if (attempt.transport) {
    return attempt.transport; // Success
  }

  // If the server cert failed validation, store a temporary override and make
  // a second attempt.
  if (encryption && attempt.certError) {
    _storeCertOverride(attempt.s, host, port);
  } else {
    throw new Error("Connection failed");
  }

  attempt = yield _attemptTransport({ host, port, encryption });
  if (attempt.transport) {
    return attempt.transport; // Success
  }

  throw new Error("Connection failed even after cert override");
});

/**
 * Try to connect and create a DevTools transport.
 *
 * @return transport DebuggerTransport
 *         A possible DevTools transport (if connection succeeded and streams
 *         are actually alive and working)
 * @return certError boolean
 *         Flag noting if cert trouble caused the streams to fail
 * @return s nsISocketTransport
 *         Underlying socket transport, in case more details are needed.
 */
let _attemptTransport = Task.async(function*({ host, port, encryption }){
  // _attemptConnect only opens the streams.  Any failures at that stage
  // aborts the connection process immedidately.
  let { s, input, output } = _attemptConnect({ host, port, encryption });

  // Check if the input stream is alive.  If encryption is enabled, we need to
  // watch out for cert errors by testing the input stream.
  let { alive, certError } = yield _isInputAlive(input);
  dumpv("Server cert accepted? " + !certError);

  let transport;
  if (alive) {
    transport = new DebuggerTransport(input, output);
  } else {
    // Something went wrong, close the streams.
    input.close();
    output.close();
  }

  return { transport, certError, s };
});

/**
 * Try to connect to a remote server socket.
 *
 * If successsful, the socket transport and its opened streams are returned.
 * Typically, this will only fail if the host / port is unreachable.  Other
 * problems, such as security errors, will allow this stage to succeed, but then
 * fail later when the streams are actually used.
 * @return s nsISocketTransport
 *         Underlying socket transport, in case more details are needed.
 * @return input nsIAsyncInputStream
 *         The socket's input stream.
 * @return output nsIAsyncOutputStream
 *         The socket's output stream.
 */
function _attemptConnect({ host, port, encryption }) {
  let s;
  if (encryption) {
    s = socketTransportService.createTransport(["ssl"], 1, host, port, null);
  } else {
    s = socketTransportService.createTransport(null, 0, host, port, null);
  }
  // By default the CONNECT socket timeout is very long, 65535 seconds,
  // so that if we race to be in CONNECT state while the server socket is still
  // initializing, the connection is stuck in connecting state for 18.20 hours!
  s.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 2);

  // openOutputStream may throw NS_ERROR_NOT_INITIALIZED if we hit some race
  // where the nsISocketTransport gets shutdown in between its instantiation and
  // the call to this method.
  let input;
  let output;
  try {
    input = s.openInputStream(0, 0, 0);
    output = s.openOutputStream(0, 0, 0);
  } catch(e) {
    DevToolsUtils.reportException("_attemptConnect", e);
    throw e;
  }

  return { s, input, output };
}

/**
 * Check if the input stream is alive.  For an encrypted connection, it may not
 * be if the client refuses the server's cert.  A cert error is expected on
 * first connection to a new host because the cert is self-signed.
 */
function _isInputAlive(input) {
  let deferred = promise.defer();
  input.asyncWait({
    onInputStreamReady(stream) {
      try {
        stream.available();
        deferred.resolve({ alive: true });
      } catch (e) {
        try {
          // getErrorClass may throw if you pass a non-NSS error
          let errorClass = nssErrorsService.getErrorClass(e.result);
          if (errorClass === Ci.nsINSSErrorsService.ERROR_CLASS_BAD_CERT) {
            deferred.resolve({ certError: true });
          } else {
            deferred.reject(e);
          }
        } catch (nssErr) {
          deferred.reject(e);
        }
      }
    }
  }, 0, 0, Services.tm.currentThread);
  return deferred.promise;
}

/**
 * To allow the connection to proceed with self-signed cert, we store a cert
 * override.  This implies that we take on the burden of authentication for
 * these connections.
 */
function _storeCertOverride(s, host, port) {
  let cert = s.securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
              .SSLStatus.serverCert;
  let overrideBits = Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                     Ci.nsICertOverrideService.ERROR_MISMATCH;
  certOverrideService.rememberValidityOverride(host, port, cert, overrideBits,
                                               true /* temporary */);
}

/**
 * Creates a new socket listener for remote connections to the DebuggerServer.
 * This helps contain and organize the parts of the server that may differ or
 * are particular to one given listener mechanism vs. another.
 */
function SocketListener() {}

SocketListener.prototype = {

  /* Socket Options */

  /**
   * The port or path to listen on.
   *
   * If given an integer, the port to listen on.  Use -1 to choose any available
   * port. Otherwise, the path to the unix socket domain file to listen on.
   */
  portOrPath: null,

  /**
   * Prompt the user to accept or decline the incoming connection. The default
   * implementation is used unless this is overridden on a particular socket
   * listener instance.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  allowConnection: prompt.Server.defaultAllowConnection,

  /**
   * Controls whether this listener is announced via the service discovery
   * mechanism.
   */
  discoverable: false,

  /**
   * Controls whether this listener's transport uses encryption.
   */
  encryption: false,

  /**
   * Controls the |Authenticator| used, which hooks various socket steps to
   * implement an authentication policy.  It is expected that different use
   * cases may override pieces of the |Authenticator|.  See auth.js.
   *
   * Here we set the default |Authenticator|, which is |Prompt|.
   */
  authenticator: new (Authenticators.get().Server)(),

  /**
   * Validate that all options have been set to a supported configuration.
   */
  _validateOptions: function() {
    if (this.portOrPath === null) {
      throw new Error("Must set a port / path to listen on.");
    }
    if (this.discoverable && !Number(this.portOrPath)) {
      throw new Error("Discovery only supported for TCP sockets.");
    }
    this.authenticator.validateOptions(this);
  },

  /**
   * Listens on the given port or socket file for remote debugger connections.
   */
  open: function() {
    this._validateOptions();
    DebuggerServer._addListener(this);

    let flags = Ci.nsIServerSocket.KeepWhenOffline;
    // A preference setting can force binding on the loopback interface.
    if (Services.prefs.getBoolPref("devtools.debugger.force-local")) {
      flags |= Ci.nsIServerSocket.LoopbackOnly;
    }

    let self = this;
    return Task.spawn(function*() {
      let backlog = 4;
      self._socket = self._createSocketInstance();
      if (self.isPortBased) {
        let port = Number(self.portOrPath);
        self._socket.initSpecialConnection(port, flags, backlog);
      } else {
        let file = nsFile(self.portOrPath);
        if (file.exists()) {
          file.remove(false);
        }
        self._socket.initWithFilename(file, parseInt("666", 8), backlog);
      }
      yield self._setAdditionalSocketOptions();
      self._socket.asyncListen(self);
      dumpn("Socket listening on: " + (self.port || self.portOrPath));
    }).then(() => {
      this._advertise();
    }).catch(e => {
      dumpn("Could not start debugging listener on '" + this.portOrPath +
            "': " + e);
      this.close();
    });
  },

  _advertise: function() {
    if (!this.discoverable || !this.port) {
      return;
    }

    let advertisement = {
      port: this.port,
      encryption: this.encryption,
    };

    this.authenticator.augmentAdvertisement(this, advertisement);

    discovery.addService("devtools", advertisement);
  },

  _createSocketInstance: function() {
    if (this.encryption) {
      return Cc["@mozilla.org/network/tls-server-socket;1"]
             .createInstance(Ci.nsITLSServerSocket);
    }
    return Cc["@mozilla.org/network/server-socket;1"]
           .createInstance(Ci.nsIServerSocket);
  },

  _setAdditionalSocketOptions: Task.async(function*() {
    if (this.encryption) {
      this._socket.serverCert = yield cert.local.getOrCreate();
      this._socket.setSessionCache(false);
      this._socket.setSessionTickets(false);
      let requestCert = Ci.nsITLSServerSocket.REQUEST_NEVER;
      this._socket.setRequestClientCertificate(requestCert);
    }
  }),

  /**
   * Closes the SocketListener.  Notifies the server to remove the listener from
   * the set of active SocketListeners.
   */
  close: function() {
    if (this.discoverable && this.port) {
      discovery.removeService("devtools");
    }
    if (this._socket) {
      this._socket.close();
      this._socket = null;
    }
    DebuggerServer._removeListener(this);
  },

  /**
   * Gets whether this listener uses a port number vs. a path.
   */
  get isPortBased() {
    return !!Number(this.portOrPath);
  },

  /**
   * Gets the port that a TCP socket listener is listening on, or null if this
   * is not a TCP socket (so there is no port).
   */
  get port() {
    if (!this.isPortBased || !this._socket) {
      return null;
    }
    return this._socket.port;
  },

  // nsIServerSocketListener implementation

  onSocketAccepted:
  DevToolsUtils.makeInfallible(function(socket, socketTransport) {
    if (this.encryption) {
      new SecurityObserver(socketTransport);
    }
    if (Services.prefs.getBoolPref("devtools.debugger.prompt-connection") &&
        !this.allowConnection()) {
      return;
    }
    dumpn("New debugging connection on " +
          socketTransport.host + ":" + socketTransport.port);

    let input = socketTransport.openInputStream(0, 0, 0);
    let output = socketTransport.openOutputStream(0, 0, 0);
    let transport = new DebuggerTransport(input, output);
    DebuggerServer._onConnection(transport);
  }, "SocketListener.onSocketAccepted"),

  onStopListening: function(socket, status) {
    dumpn("onStopListening, status: " + status);
  }

};

// Client must complete TLS handshake within this window (ms)
loader.lazyGetter(this, "HANDSHAKE_TIMEOUT", () => {
  return Services.prefs.getIntPref("devtools.remote.tls-handshake-timeout");
});

function SecurityObserver(socketTransport) {
  this.socketTransport = socketTransport;
  let connectionInfo = socketTransport.securityInfo
                       .QueryInterface(Ci.nsITLSServerConnectionInfo);
  connectionInfo.setSecurityObserver(this);
  this._handshakeTimeout = setTimeout(this._onHandshakeTimeout.bind(this),
                                      HANDSHAKE_TIMEOUT);
}

SecurityObserver.prototype = {

  _onHandshakeTimeout() {
    dumpv("Client failed to complete handshake");
    this.destroy(Cr.NS_ERROR_NET_TIMEOUT);
  },

  // nsITLSServerSecurityObserver implementation
  onHandshakeDone(socket, clientStatus) {
    clearTimeout(this._handshakeTimeout);
    dumpv("TLS version:    " + clientStatus.tlsVersionUsed.toString(16));
    dumpv("TLS cipher:     " + clientStatus.cipherName);
    dumpv("TLS key length: " + clientStatus.keyLength);
    dumpv("TLS MAC length: " + clientStatus.macLength);
    /*
     * TODO: These rules should be really be set on the TLS socket directly, but
     * this would need more platform work to expose it via XPCOM.
     *
     * Server *will* send hello packet when any rules below are not met, but the
     * socket then closes after that.
     *
     * Enforcing cipher suites here would be a bad idea, as we want TLS
     * cipher negotiation to work correctly.  The server already allows only
     * Gecko's normal set of cipher suites.
     */
    if (clientStatus.tlsVersionUsed != Ci.nsITLSClientStatus.TLS_VERSION_1_2) {
      this.destroy(Cr.NS_ERROR_CONNECTION_REFUSED);
    }
  },

  destroy(result) {
    clearTimeout(this._handshakeTimeout);
    let connectionInfo = this.socketTransport.securityInfo
                         .QueryInterface(Ci.nsITLSServerConnectionInfo);
    connectionInfo.setSecurityObserver(null);
    this.socketTransport.close(result);
    this.socketTransport = null;
  }

};

DebuggerSocket.createListener = function() {
  return new SocketListener();
};

exports.DebuggerSocket = DebuggerSocket;
