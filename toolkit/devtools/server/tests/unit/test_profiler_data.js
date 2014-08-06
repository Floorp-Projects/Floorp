/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the profiler actor can correctly retrieve a profile after
 * it is activated.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const INITIAL_WAIT_TIME = 100; // ms
const MAX_WAIT_TIME = 20000; // ms

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
      test_data(client, actor, () => {
        deactivate_profiler(client, actor, () => {
          client.close(do_test_finished);
        })
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

function deactivate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "stopProfiler" }, response => {
    do_check_false(response.started);
    client.request({ to: actor, type: "isActive" }, response => {
      do_check_false(response.isActive);
      callback();
    });
  });
}

function test_data(client, actor, callback)
{
  function attempt(delay)
  {
    // No idea why, but Components.stack.sourceLine returns null.
    let funcLine = Components.stack.lineNumber - 3;

    // Spin for the requested time, then take a sample.
    let start = Date.now();
    let stack;
    do_print("Attempt: delay = " + delay);
    while (Date.now() - start < delay) { stack = Components.stack; }
    do_print("Attempt: finished waiting.");

    client.request({ to: actor, type: "getProfile" }, response => {
      // Any valid getProfile response should have the following top
      // level structure.
      do_check_eq(typeof response.profile, "object");
      do_check_eq(typeof response.profile.meta, "object");
      do_check_eq(typeof response.profile.meta.platform, "string");
      do_check_eq(typeof response.profile.threads, "object");
      do_check_eq(typeof response.profile.threads[0], "object");
      do_check_eq(typeof response.profile.threads[0].samples, "object");

      // At this point, we may or may not have samples, depending on
      // whether the spin loop above has given the profiler enough time
      // to get started.
      if (response.profile.threads[0].samples.length == 0) {
        if (delay < MAX_WAIT_TIME) {
          // Double the spin-wait time and try again.
          do_print("Attempt: no samples, going around again.");
          return attempt(delay * 2);
        } else {
          // We've waited long enough, so just fail.
          do_print("Attempt: waited a long time, but no samples were collected.");
          do_print("Giving up.");
          do_check_true(false);
          return;
        }
      }

      // Now check the samples. At least one sample is expected to
      // have been in the busy wait above.
      let loc = stack.name + " (" + stack.filename + ":" + funcLine + ")";
      let line = stack.lineNumber;

      do_check_true(response.profile.threads[0].samples.some(sample => {
        return typeof sample.frames == "object" &&
               sample.frames.length != 0 &&
               sample.frames.some(f => (f.line == line) && (f.location == loc));
      }));

      callback();
    });
  }

  // Start off with a 100 millisecond delay.
  attempt(INITIAL_WAIT_TIME);
}
