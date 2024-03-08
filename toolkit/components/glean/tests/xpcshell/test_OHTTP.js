/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  // FOG needs a profile dir to put its data in.
  do_get_profile();

  PingServer.start();

  registerCleanupFunction(async () => {
    await PingServer.stop();
  });

  Services.prefs.setIntPref(
    "telemetry.fog.test.localhost_port",
    PingServer.port
  );
  // Port pref needs to be set before init, so let's reset to reinit.
  Services.fog.testResetFOG();
});

add_task(async () => {
  PingServer.clearRequests();
  GleanPings.testOhttpPing.submit();

  let ping = await PingServer.promiseNextPing();

  ok(!("client_info" in ping), "No client_info allowed.");
  ok(!("ping_info" in ping), "No ping_info allowed.");
});
