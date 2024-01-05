/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that AsyncShutdown report errors correctly

// Note: these functions are evaluated in their own process, hence the need
// to import modules into each function.

function setup_crash() {
  const { AsyncShutdown } = ChromeUtils.importESModule(
    "resource://gre/modules/AsyncShutdown.sys.mjs"
  );

  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 10);

  let TOPIC = "testing-async-shutdown-crash";
  let phase = AsyncShutdown._getPhase(TOPIC);
  phase.addBlocker("A blocker that is never satisfied", function () {
    dump("Installing blocker\n");
    let deferred = Promise.withResolvers();
    return deferred.promise;
  });

  Services.obs.notifyObservers(null, TOPIC);
  dump(new Error().stack + "\n");
  dump("Waiting for crash\n");
}

function after_crash(mdump, extra) {
  info("after crash: " + extra.AsyncShutdownTimeout);
  let data = JSON.parse(extra.AsyncShutdownTimeout);
  Assert.equal(data.phase, "testing-async-shutdown-crash");
  Assert.equal(data.conditions[0].name, "A blocker that is never satisfied");
  // This test spawns subprocesses by using argument "-e" of xpcshell, so
  // this is the filename known to xpcshell.
  Assert.equal(data.conditions[0].filename, "-e");
}

// Test that AsyncShutdown + IOUtils reports errors correctly.,

function setup_ioutils_crash() {
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 1);

  IOUtils.profileBeforeChange.addBlocker(
    "Adding a blocker that will never be resolved",
    () => Promise.withResolvers().promise
  );

  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN
  );
  dump("Waiting for crash\n");
}

function after_ioutils_crash(mdump, extra) {
  info("after IOUtils crash: " + extra.AsyncShutdownTimeout);
  let data = JSON.parse(extra.AsyncShutdownTimeout);
  Assert.equal(
    data.phase,
    "IOUtils: waiting for profileBeforeChange IO to complete"
  );
}

add_task(async function run_test() {
  await do_crash(setup_crash, after_crash);
  await do_crash(setup_ioutils_crash, after_ioutils_crash);
});
