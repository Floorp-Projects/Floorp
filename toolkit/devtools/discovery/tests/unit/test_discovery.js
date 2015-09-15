/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cu = Components.utils;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
Services.prefs.setBoolPref("devtools.discovery.log", true);

do_register_cleanup(() => {
  Services.prefs.clearUserPref("devtools.discovery.log");
});

const { require } =
  Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const promise = require("promise");
const EventEmitter = require("devtools/toolkit/event-emitter");
const discovery = require("devtools/toolkit/discovery/discovery");
const { setTimeout, clearTimeout } = require("sdk/timers");

function log(msg) {
  do_print("DISCOVERY: " + msg);
}

// Global map of actively listening ports to TestTransport instances
var gTestTransports = {};

/**
 * Implements the same API as Transport in discovery.js.  Here, no UDP sockets
 * are used.  Instead, messages are delivered immediately.
 */
function TestTransport(port) {
  EventEmitter.decorate(this);
  this.port = port;
  gTestTransports[this.port] = this;
}

TestTransport.prototype = {

  send: function(object, port) {
    log("Send to " + port + ":\n" + JSON.stringify(object, null, 2));
    if (!gTestTransports[port]) {
      log("No listener on port " + port);
      return;
    }
    let message = JSON.stringify(object);
    gTestTransports[port].onPacketReceived(null, message);
  },

  destroy: function() {
    delete gTestTransports[this.port];
  },

  // nsIUDPSocketListener

  onPacketReceived: function(socket, message) {
    let object = JSON.parse(message);
    object.from = "localhost";
    log("Recv on " + this.port + ":\n" + JSON.stringify(object, null, 2));
    this.emit("message", object);
  },

  onStopListening: function(socket, status) {}

};

// Use TestTransport instead of the usual Transport
discovery._factories.Transport = TestTransport;

// Ignore name generation on b2g and force a fixed value
Object.defineProperty(discovery.device, "name", {
  get: function() {
    return "test-device";
  }
});

function run_test() {
  run_next_test();
}

add_task(function*() {
  // At startup, no remote devices are known
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), []);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  discovery.scan();

  // No services added yet, still empty
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), []);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  discovery.addService("devtools", { port: 1234 });

  // Changes not visible until next scan
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), []);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  yield scanForChange("devtools", "added");

  // Now we see the new service
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), ["test-device"]);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  discovery.addService("penguins", { tux: true });
  yield scanForChange("penguins", "added");

  deepEqual(discovery.getRemoteDevicesWithService("devtools"), ["test-device"]);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), ["test-device"]);
  deepEqual(discovery.getRemoteDevices(), ["test-device"]);

  deepEqual(discovery.getRemoteService("devtools", "test-device"),
            { port: 1234, host: "localhost" });
  deepEqual(discovery.getRemoteService("penguins", "test-device"),
            { tux: true,  host: "localhost" });

  discovery.removeService("devtools");
  yield scanForChange("devtools", "removed");

  discovery.addService("penguins", { tux: false });
  yield scanForChange("penguins", "updated");

  // Scan again, but nothing should be removed
  yield scanForNoChange("penguins", "removed");

  // Split the scanning side from the service side to simulate the machine with
  // the service becoming unreachable
  gTestTransports = {};

  discovery.removeService("penguins");
  yield scanForChange("penguins", "removed");
});

function scanForChange(service, changeType) {
  let deferred = promise.defer();
  let timer = setTimeout(() => {
    deferred.reject(new Error("Reply never arrived"));
  }, discovery.replyTimeout + 500);
  discovery.on(service + "-device-" + changeType, function onChange() {
    discovery.off(service + "-device-" + changeType, onChange);
    clearTimeout(timer);
    deferred.resolve();
  });
  discovery.scan();
  return deferred.promise;
}

function scanForNoChange(service, changeType) {
  let deferred = promise.defer();
  let timer = setTimeout(() => {
    deferred.resolve();
  }, discovery.replyTimeout + 500);
  discovery.on(service + "-device-" + changeType, function onChange() {
    discovery.off(service + "-device-" + changeType, onChange);
    clearTimeout(timer);
    deferred.reject(new Error("Unexpected change occurred"));
  });
  discovery.scan();
  return deferred.promise;
}
