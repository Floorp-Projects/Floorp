/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the profiler module is kept active when there are multiple
 * client consumers and one requests deactivation.
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

  connect_client((client1, actor1) => {
    connect_client((client2, actor2) => {
      test_close(client1, actor1, client2, actor2, () => {
        client1.close(() => {
          client2.close(() => {
            do_test_finished();
          });
        });
      });
    });
  });

  do_test_pending();
}

function activate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "startProfiler" }, response => {
    do_check_true(response.started);
    do_check_true(Profiler.IsActive());

    client.request({ to: actor, type: "isActive" }, response => {
      do_check_true(response.isActive);
      callback();
    });
  });
}

function deactivate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "stopProfiler" }, response => {
    do_check_false(response.started);
    do_check_true(Profiler.IsActive());

    client.request({ to: actor, type: "isActive" }, response => {
      do_check_true(response.isActive);
      callback();
    });
  });
}

function test_close(client1, actor1, client2, actor2, callback)
{
  activate_profiler(client1, actor1, () => {
    activate_profiler(client2, actor2, () => {
      deactivate_profiler(client1, actor1, () => {
        deactivate_profiler(client2, actor2, () => {
          callback();
        });
      });
    });
  });
}
