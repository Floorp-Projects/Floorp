/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the event notification service for the profiler actor, specifically
 * for when the profiler is started and stopped.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);

function connect_client(callback)
{
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(() => {
    client.listTabs(response => {
      callback(client, response.profilerActor);
    });
  });
}

function run_test()
{
  DebuggerServer.init(() => true);
  DebuggerServer.addBrowserActors();

  connect_client((client, actor) => {
    activate_profiler(client, actor, () => {
      test_events(client, actor, () => {
        client.close(do_test_finished);
      });
    });
  })

  do_test_pending();
}

function activate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "startProfiler" }, response => {
    do_check_true(response.started);
    client.request({ to: actor, type: "isActive" }, response => {
      do_check_true(response.isActive);
      callback();
    });
  });
}

function register_events(client, actor, events, callback)
{
  client.request({
    to: actor,
    type: "registerEventNotifications",
    events: events
  }, callback);
}

function wait_for_event(client, topic, callback)
{
  client.addListener("eventNotification", (type, response) => {
    do_check_eq(type, "eventNotification");

    if (response.topic == topic) {
      callback();
    }
  });
}

function test_events(client, actor, callback)
{
  register_events(client, actor, ["profiler-started", "profiler-stopped"], () => {
    wait_for_event(client, "profiler-started", () => {
      wait_for_event(client, "profiler-stopped", () => {
        callback();
      });
      Profiler.StopProfiler();
    });
    Profiler.StartProfiler(1000000, 1, ["js"], 1);
  });
}
