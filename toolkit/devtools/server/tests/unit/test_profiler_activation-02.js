/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the profiler actor correctly handles the case where the
 * built-in module was already started.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const WAIT_TIME = 1000; // ms

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
  // Ensure the profiler is already running when the test starts.
  Profiler.StartProfiler(1000000, 1, ["js"], 1);

  DevToolsUtils.waitForTime(WAIT_TIME).then(() => {
    DebuggerServer.init(() => true);
    DebuggerServer.addBrowserActors();

    connect_client((client, actor) => {
      test_start_time(client, actor, () => {
        client.close(do_test_finished);
      });
    });
  });

  do_test_pending();
}

function test_start_time(client, actor, callback) {
  // Profiler should already be active at this point.
  client.request({ to: actor, type: "isActive" }, firstResponse => {
    do_check_true(Profiler.IsActive());
    do_check_true(firstResponse.isActive);
    do_check_true(firstResponse.currentTime > 0);

    client.request({ to: actor, type: "getProfile" }, secondResponse => {
      do_check_true("profile" in secondResponse);
      do_check_true(secondResponse.currentTime > firstResponse.currentTime);

      callback();
    });
  });
}
