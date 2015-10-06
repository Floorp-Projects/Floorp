/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Constructor: CC, classes: Cc, interfaces: Ci, utils: Cu} = Components;

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
const ServerSocket = CC("@mozilla.org/network/server-socket;1", "nsIServerSocket", "initSpecialConnection");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/dispatcher.js");
Cu.import("chrome://marionette/content/driver.js");
Cu.import("chrome://marionette/content/elements.js");
Cu.import("chrome://marionette/content/simpletest.js");

// Bug 1083711: Load transport.js as an SDK module instead of subscript
loader.loadSubScript("resource://gre/modules/devtools/shared/transport/transport.js");

// Preserve this import order:
var events = {};
loader.loadSubScript("chrome://marionette/content/EventUtils.js", events);
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", events);
loader.loadSubScript("chrome://marionette/content/frame-manager.js");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["MarionetteServer"];
const CONTENT_LISTENER_PREF = "marionette.contentListener";

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
 * Function that takes an Emulator and produces a GeckoDriver.
 *
 * Determines application name and device type to initialise the driver
 * with.  Also bypasses offline status if the device is a qemu or panda
 * type device.
 *
 * @return {GeckoDriver}
 *     A driver instance.
 */
MarionetteServer.prototype.driverFactory = function(emulator) {
  let appName = isMulet() ? "B2G" : Services.appinfo.name;
  let qemu = "0";
  let device = null;
  let bypassOffline = false;

  try {
    Cu.import("resource://gre/modules/systemlibs.js");
    qemu = libcutils.property_get("ro.kernel.qemu");
    logger.debug("B2G emulator: " + (qemu == "1" ? "yes" : "no"));
    device = libcutils.property_get("ro.product.device");
    logger.debug("Device detected is " + device);
    bypassOffline = (qemu == "1" || device == "panda");
  } catch (e) {}

  if (qemu == "1") {
    device = "qemu";
  }
  if (!device) {
    device = "desktop";
  }

  Services.prefs.setBoolPref(CONTENT_LISTENER_PREF, false);

  if (bypassOffline) {
    logger.debug("Bypassing offline status");
    Services.prefs.setBoolPref("network.gonk.manage-offline-status", false);
    Services.io.manageOfflineStatus = false;
    Services.io.offline = false;
  }

  return new GeckoDriver(appName, device, emulator);
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

  let stopSignal = () => this.stop();
  let dispatcher = new Dispatcher(connId, transport, this.driverFactory, stopSignal);
  dispatcher.onclose = this.onConnectionClosed.bind(this);
  this.conns[connId] = dispatcher;

  logger.info(`Accepted connection ${connId} from ${clientSocket.host}:${clientSocket.port}`);

  // Create a root actor for the connection and send the hello packet
  dispatcher.sayHello();
  transport.ready();
};

MarionetteServer.prototype.onConnectionClosed = function(conn) {
  let id = conn.id;
  delete this.conns[id];
  logger.info(`Closed connection ${id}`);
};

function isMulet() {
  try {
    return Services.prefs.getBoolPref("b2g.is_mulet");
  } catch (e) {
    return false;
  }
}
