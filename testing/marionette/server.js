/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {Constructor: CC, classes: Cc, interfaces: Ci, utils: Cu} = Components;

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
const ServerSocket = CC("@mozilla.org/network/server-socket;1", "nsIServerSocket", "initSpecialConnection");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/dispatcher.js");
Cu.import("chrome://marionette/content/driver.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/simpletest.js");

// Bug 1083711: Load transport.js as an SDK module instead of subscript
loader.loadSubScript("resource://devtools/shared/transport/transport.js");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["MarionetteServer"];

const CONTENT_LISTENER_PREF = "marionette.contentListener";
const MANAGE_OFFLINE_STATUS_PREF = "network.gonk.manage-offline-status";

/**
 * Bootstraps Marionette and handles incoming client connections.
 *
 * Once started, it opens a TCP socket sporting the debugger transport
 * protocol on the provided port.  For every new client a Dispatcher is
 * created.
 *
 * @param {number} port
 *     Port for server to listen to.
 * @param {boolean} forceLocal
 *     Listen only to connections from loopback if true.  If false,
 *     accept all connections.
 */
this.MarionetteServer = function(port, forceLocal) {
  this.port = port;
  this.forceLocal = forceLocal;
  this.conns = {};
  this.nextConnId = 0;
  this.alive = false;
};

/**
 * Function produces a GeckoDriver.
 *
 * Determines application name and device type to initialise the driver
 * with.
 *
 * @return {GeckoDriver}
 *     A driver instance.
 */
MarionetteServer.prototype.driverFactory = function() {
  let appName = isMulet() ? "B2G" : Services.appinfo.name;
  let device = null;
  let bypassOffline = false;

  if (!device) {
    device = "desktop";
  }

  Preferences.set(CONTENT_LISTENER_PREF, false);

  if (bypassOffline) {
      logger.debug("Bypassing offline status");
      Preferences.set(MANAGE_OFFLINE_STATUS_PREF, false);
      Services.io.manageOfflineStatus = false;
      Services.io.offline = false;
  }

  let stopSignal = () => this.stop();
  return new GeckoDriver(appName, device, stopSignal);
};

MarionetteServer.prototype.start = function() {
  if (this.alive) {
    return;
  }
  let flags = Ci.nsIServerSocket.KeepWhenOffline;
  if (this.forceLocal) {
    flags |= Ci.nsIServerSocket.LoopbackOnly;
  }
  this.listener = new ServerSocket(this.port, flags, 0);
  this.listener.asyncListen(this);
  this.alive = true;
};

MarionetteServer.prototype.stop = function() {
  if (!this.alive) {
    return;
  }
  this.closeListener();
  this.alive = false;
};

MarionetteServer.prototype.closeListener = function() {
  this.listener.close();
  this.listener = null;
};

MarionetteServer.prototype.onSocketAccepted = function(
    serverSocket, clientSocket) {
  let input = clientSocket.openInputStream(0, 0, 0);
  let output = clientSocket.openOutputStream(0, 0, 0);
  let transport = new DebuggerTransport(input, output);
  let connId = "conn" + this.nextConnId++;

  let dispatcher = new Dispatcher(connId, transport, this.driverFactory.bind(this));
  dispatcher.onclose = this.onConnectionClosed.bind(this);
  this.conns[connId] = dispatcher;

  logger.debug(`Accepted connection ${connId} from ${clientSocket.host}:${clientSocket.port}`);
  dispatcher.sayHello();
  transport.ready();
};

MarionetteServer.prototype.onConnectionClosed = function(conn) {
  let id = conn.connId;
  delete this.conns[id];
  logger.debug(`Closed connection ${id}`);
};

function isMulet() {
  return Preferences.get("b2g.is_mulet", false);
}
