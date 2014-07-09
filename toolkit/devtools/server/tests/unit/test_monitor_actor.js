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
  MonitorClient.prototype.start = function () {
    this.client.request({
      to: this.actor,
      type: "start"
    });
  }
  MonitorClient.prototype.stop = function (callback) {
    this.client.request({
      to: this.actor,
      type: "stop"
    }, callback);
  }

  let monitor;

  // Start tracking event loop lags.
  client.connect(function () {
    client.listTabs(function(resp) {
      monitor = new MonitorClient(client, resp);
      monitor.start();
      monitor.on("update", gotUpdate);
      do_execute_soon(update);
    });
  });

  let time = new Date().getTime();

  function update() {
    let event = {
      time: time,
      value: 42
    };
    Services.obs.notifyObservers(null, "devtools-monitor-update", JSON.stringify(event));
  }

  function gotUpdate(type, data) {
    do_check_eq(data.length, 1);
    let evt = data[0];
    do_check_eq(evt.value, 42);
    do_check_eq(evt.time, time);
    monitor.stop(function (aResponse) {
      monitor.destroy();
      finishClient(client);
    });
  }

  do_test_pending();
}
