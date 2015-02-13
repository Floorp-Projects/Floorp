/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the profiler responds to "getSharedLibraryInformation" adequately.
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
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  connect_client((client, actor) => {
    test_getsharedlibraryinformation(client, actor, () => {
      client.close(() => {
        do_test_finished();
      });
    });
  });

  do_test_pending();
}

function test_getsharedlibraryinformation(client, actor, callback)
{
  client.request({ to: actor, type: "getSharedLibraryInformation" }, response => {
    do_check_eq(typeof response.sharedLibraryInformation, "string");
    let libs = [];
    try {
      libs = JSON.parse(response.sharedLibraryInformation);
    } catch (e) {
      do_check_true(false);
    }
    do_check_eq(typeof libs, "object");
    do_check_true(libs.length >= 1);
    do_check_eq(typeof libs[0], "object");
    do_check_eq(typeof libs[0].name, "string");
    do_check_eq(typeof libs[0].start, "number");
    do_check_eq(typeof libs[0].end, "number");
    do_check_true(libs[0].start <= libs[0].end);
    callback();
  });
}
