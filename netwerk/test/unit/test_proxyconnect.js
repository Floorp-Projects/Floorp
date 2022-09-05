/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// test_connectonly tests happy path of proxy connect
// 1. CONNECT to localhost:socketserver_port
// 2. Write 200 Connection established
// 3. Write data to the tunnel (and read server-side)
// 4. Read data from the tunnel (and write server-side)
// 5. done
// test_connectonly_noproxy tests an http channel with only connect set but
// no proxy configured.
// 1. OnTransportAvailable callback NOT called (checked in step 2)
// 2. StopRequest callback called
// 3. done
// test_connectonly_nonhttp tests an http channel with only connect set with a
// non-http proxy.
// 1. OnTransportAvailable callback NOT called (checked in step 2)
// 2. StopRequest callback called
// 3. done

// -1 then initialized with an actual port from the serversocket
"use strict";

var socketserver_port = -1;

const CC = Components.Constructor;
const ServerSocket = CC(
  "@mozilla.org/network/server-socket;1",
  "nsIServerSocket",
  "init"
);
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);
const BinaryOutputStream = CC(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);

const STATE_NONE = 0;
const STATE_READ_CONNECT_REQUEST = 1;
const STATE_WRITE_CONNECTION_ESTABLISHED = 2;
const STATE_CHECK_WRITE = 3; // write to the tunnel
const STATE_CHECK_WRITE_READ = 4; // wrote to the tunnel, check connection data
const STATE_CHECK_READ = 5; // read from the tunnel
const STATE_CHECK_READ_WROTE = 6; // wrote to connection, check tunnel data
const STATE_COMPLETED = 100;

const CONNECT_RESPONSE_STRING = "HTTP/1.1 200 Connection established\r\n\r\n";
const CHECK_WRITE_STRING = "hello";
const CHECK_READ_STRING = "world";
const ALPN = "webrtc";

var connectRequest = "";
var checkWriteData = "";
var checkReadData = "";

var threadManager;
var socket;
var streamIn;
var streamOut;
var accepted = false;
var acceptedSocket;
var state = STATE_NONE;
var transportAvailable = false;
var proxiedChannel;
var listener = {
  expectedCode: -1, // uninitialized

  onStartRequest: function test_onStartR(request) {},

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    if (state === STATE_COMPLETED) {
      Assert.equal(transportAvailable, false, "transport available not called");
      Assert.equal(status, 0x80004005, "error code matches");
      Assert.equal(proxiedChannel.httpProxyConnectResponseCode, 200);
      nextTest();
      return;
    }

    Assert.equal(accepted, true, "socket accepted");
    accepted = false;
  },
};

var upgradeListener = {
  onTransportAvailable: (transport, socketIn, socketOut) => {
    if (!transport || !socketIn || !socketOut) {
      do_throw("on transport available failed");
    }

    if (state !== STATE_CHECK_WRITE) {
      do_throw("bad state");
    }

    transportAvailable = true;

    socketIn.asyncWait(connectHandler, 0, 0, threadManager.mainThread);
    socketOut.asyncWait(connectHandler, 0, 0, threadManager.mainThread);
  },
  QueryInterface: ChromeUtils.generateQI(["nsIHttpUpgradeListener"]),
};

var connectHandler = {
  onInputStreamReady: input => {
    try {
      const bis = new BinaryInputStream(input);
      var data = bis.readByteArray(input.available());

      dataAvailable(data);

      if (state !== STATE_COMPLETED) {
        input.asyncWait(connectHandler, 0, 0, threadManager.mainThread);
      }
    } catch (e) {
      do_throw(e);
    }
  },
  onOutputStreamReady: output => {
    writeData(output);
  },
  QueryInterface: iid => {
    if (
      iid.equals(Ci.nsISupports) ||
      iid.equals(Ci.nsIInputStreamCallback) ||
      iid.equals(Ci.nsIOutputStreamCallback)
    ) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },
};

function dataAvailable(data) {
  switch (state) {
    case STATE_READ_CONNECT_REQUEST:
      connectRequest += String.fromCharCode.apply(String, data);
      const headerEnding = connectRequest.indexOf("\r\n\r\n");
      const alpnHeaderIndex = connectRequest.indexOf(`ALPN: ${ALPN}`);

      if (headerEnding != -1) {
        const requestLine = `CONNECT localhost:${socketserver_port} HTTP/1.1`;
        Assert.equal(connectRequest.indexOf(requestLine), 0, "connect request");
        Assert.equal(headerEnding, connectRequest.length - 4, "req head only");
        Assert.notEqual(alpnHeaderIndex, -1, "alpn header found");

        state = STATE_WRITE_CONNECTION_ESTABLISHED;
        streamOut.asyncWait(connectHandler, 0, 0, threadManager.mainThread);
      }

      break;
    case STATE_CHECK_WRITE_READ:
      checkWriteData += String.fromCharCode.apply(String, data);

      if (checkWriteData.length >= CHECK_WRITE_STRING.length) {
        Assert.equal(checkWriteData, CHECK_WRITE_STRING, "correct write data");

        state = STATE_CHECK_READ;
        streamOut.asyncWait(connectHandler, 0, 0, threadManager.mainThread);
      }

      break;
    case STATE_CHECK_READ_WROTE:
      checkReadData += String.fromCharCode.apply(String, data);

      if (checkReadData.length >= CHECK_READ_STRING.length) {
        Assert.equal(checkReadData, CHECK_READ_STRING, "correct read data");

        state = STATE_COMPLETED;

        streamIn.asyncWait(null, 0, 0, null);
        acceptedSocket.close(0);

        nextTest();
      }

      break;
    default:
      do_throw("bad state: " + state);
  }
}

function writeData(output) {
  let bos = new BinaryOutputStream(output);

  switch (state) {
    case STATE_WRITE_CONNECTION_ESTABLISHED:
      bos.write(CONNECT_RESPONSE_STRING, CONNECT_RESPONSE_STRING.length);
      state = STATE_CHECK_WRITE;
      break;
    case STATE_CHECK_READ:
      bos.write(CHECK_READ_STRING, CHECK_READ_STRING.length);
      state = STATE_CHECK_READ_WROTE;
      break;
    case STATE_CHECK_WRITE:
      bos.write(CHECK_WRITE_STRING, CHECK_WRITE_STRING.length);
      state = STATE_CHECK_WRITE_READ;
      break;
    default:
      do_throw("bad state: " + state);
  }
}

function makeChan(url) {
  if (!url) {
    url = "https://localhost:" + socketserver_port + "/";
  }

  var flags =
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL |
    Ci.nsILoadInfo.SEC_DONT_FOLLOW_REDIRECTS |
    Ci.nsILoadInfo.SEC_COOKIES_OMIT;

  var chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    securityFlags: flags,
  });
  chan = chan.QueryInterface(Ci.nsIHttpChannel);

  var internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  internal.HTTPUpgrade(ALPN, upgradeListener);
  internal.setConnectOnly();

  return chan;
}

function socketAccepted(socket, transport) {
  accepted = true;

  // copied from httpd.js
  const SEGMENT_SIZE = 8192;
  const SEGMENT_COUNT = 1024;

  switch (state) {
    case STATE_NONE:
      state = STATE_READ_CONNECT_REQUEST;
      break;
    default:
      return;
  }

  acceptedSocket = transport;

  try {
    streamIn = transport
      .openInputStream(0, SEGMENT_SIZE, SEGMENT_COUNT)
      .QueryInterface(Ci.nsIAsyncInputStream);
    streamOut = transport.openOutputStream(0, 0, 0);

    streamIn.asyncWait(connectHandler, 0, 0, threadManager.mainThread);
  } catch (e) {
    transport.close(Cr.NS_BINDING_ABORTED);
    do_throw(e);
  }
}

function stopListening(socket, status) {
  if (tests && tests.length !== 0 && do_throw) {
    do_throw("should never stop");
  }
}

function createProxy() {
  try {
    threadManager = Cc["@mozilla.org/thread-manager;1"].getService();

    socket = new ServerSocket(-1, true, 1);
    socketserver_port = socket.port;

    socket.asyncListen({
      onSocketAccepted: socketAccepted,
      onStopListening: stopListening,
    });
  } catch (e) {
    do_throw(e);
  }
}

function test_connectonly() {
  Services.prefs.setCharPref("network.proxy.ssl", "localhost");
  Services.prefs.setIntPref("network.proxy.ssl_port", socketserver_port);
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
  Services.prefs.setIntPref("network.proxy.type", 1);

  var chan = makeChan();
  proxiedChannel = chan.QueryInterface(Ci.nsIProxiedChannel);
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_connectonly_noproxy() {
  clearPrefs();
  var chan = makeChan();
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_connectonly_nonhttp() {
  clearPrefs();

  Services.prefs.setCharPref("network.proxy.socks", "localhost");
  Services.prefs.setIntPref("network.proxy.socks_port", socketserver_port);
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
  Services.prefs.setIntPref("network.proxy.type", 1);

  var chan = makeChan();
  chan.asyncOpen(listener);

  do_test_pending();
}

function nextTest() {
  transportAvailable = false;

  if (!tests.length) {
    do_test_finished();
    return;
  }

  tests.shift()();
  do_test_finished();
}

var tests = [
  test_connectonly,
  test_connectonly_noproxy,
  test_connectonly_nonhttp,
];

function clearPrefs() {
  Services.prefs.clearUserPref("network.proxy.ssl");
  Services.prefs.clearUserPref("network.proxy.ssl_port");
  Services.prefs.clearUserPref("network.proxy.socks");
  Services.prefs.clearUserPref("network.proxy.socks_port");
  Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
  Services.prefs.clearUserPref("network.proxy.type");
}

function run_test() {
  createProxy();

  registerCleanupFunction(clearPrefs);

  nextTest();
  do_test_pending();
}
