/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the profiler module and actor have the correct state on
 * initialization, activation, and when a clients' connection closes.
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
  // Ensure the profiler is not running when the test starts (it could
  // happen if the MOZ_PROFILER_STARTUP environment variable is set).
  Profiler.StopProfiler();

  DebuggerServer.init(() => true);
  DebuggerServer.addBrowserActors();

  connect_client((client1, actor1) => {
    connect_client((client2, actor2) => {
      test_activate(client1, actor1, client2, actor2, () => {
        do_test_finished();
      });
    });
  });

  do_test_pending();
}

function test_activate(client1, actor1, client2, actor2, callback) {
  // Profiler should be inactive at this point.
  client1.request({ to: actor1, type: "isActive" }, response => {
    do_check_false(Profiler.IsActive());
    do_check_false(response.isActive);
    do_check_eq(response.currentTime, undefined);

    // Start the profiler on the first connection....
    client1.request({ to: actor1, type: "startProfiler" }, response => {
      do_check_true(Profiler.IsActive());
      do_check_true(response.started);

      // On the next connection just make sure the actor has been instantiated.
      client2.request({ to: actor2, type: "isActive" }, response => {
        do_check_true(Profiler.IsActive());
        do_check_true(response.isActive);
        do_check_true(response.currentTime > 0);

        let origConnectionClosed = DebuggerServer._connectionClosed;

        DebuggerServer._connectionClosed = function(conn) {
          origConnectionClosed.call(this, conn);

          // The first client is the only actor that started the profiler,
          // however the second client can request the accumulated profile data
          // at any moment, so the profiler module shouldn't have deactivated.
          do_check_true(Profiler.IsActive());

          DebuggerServer._connectionClosed = function(conn) {
            origConnectionClosed.call(this, conn);

            // Now there are no open clients at all, it should *definitely*
            // be deactivated by now.
            do_check_false(Profiler.IsActive());

            callback();
          }
          client2.close();
        };
        client1.close();
      });
    });
  });
}
