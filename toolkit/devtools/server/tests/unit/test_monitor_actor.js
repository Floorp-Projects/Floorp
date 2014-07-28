/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the monitor actor.
 */

"use strict";

function run_test()
{
  let EventEmitter = devtools.require("devtools/toolkit/event-emitter");

  DebuggerServer.init(function () { return true; });
  DebuggerServer.addBrowserActors();

  let client = new DebuggerClient(DebuggerServer.connectPipe());

  function MonitorClient(client, form) {
    this.client = client;
    this.actor = form.monitorActor;
    this.events = ["update"];

    EventEmitter.decorate(this);
    client.registerClient(this);
  }
  MonitorClient.prototype.destroy = function () {
    client.unregisterClient(this);
  }
  MonitorClient.prototype.detach = function () {}
  MonitorClient.prototype.start = function (callback) {
    this.client.request({
      to: this.actor,
      type: "start"
    }, callback);
  }
  MonitorClient.prototype.stop = function (callback) {
    this.client.request({
      to: this.actor,
      type: "stop"
    }, callback);
  }

  let monitor;

  // Start the monitor actor.
  client.connect(function () {
    client.listTabs(function(resp) {
      monitor = new MonitorClient(client, resp);
      monitor.on("update", gotUpdate);
      monitor.start(update);
    });
  });

  let time = Date.now();

  function update() {
    let event = {
      graph: "Test",
      curve: "test",
      value: 42,
      time: time,
    };
    Services.obs.notifyObservers(null, "devtools-monitor-update", JSON.stringify(event));
  }

  function gotUpdate(type, packet) {
    packet.data.forEach(function(event) {
      // Ignore updates that were not sent by this test.
      if (event.graph === "Test") {
        do_check_eq(event.curve, "test");
        do_check_eq(event.value, 42);
        do_check_eq(event.time, time);
        monitor.stop(function (aResponse) {
          monitor.destroy();
          finishClient(client);
        });
      }
    });
  }

  do_test_pending();
}
