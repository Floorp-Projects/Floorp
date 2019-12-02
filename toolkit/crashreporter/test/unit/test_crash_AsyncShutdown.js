/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that AsyncShutdown report errors correctly

// Note: these functions are evaluated in their own process, hence the need
// to import modules into each function.

function setup_crash() {
  /* global AsyncShutdown */
  ChromeUtils.import("resource://gre/modules/AsyncShutdown.jsm", this);
  ChromeUtils.import("resource://gre/modules/Services.jsm", this);
  ChromeUtils.import("resource://gre/modules/Promise.jsm", this);

  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 10);

  let TOPIC = "testing-async-shutdown-crash";
  let phase = AsyncShutdown._getPhase(TOPIC);
  phase.addBlocker("A blocker that is never satisfied", function() {
    dump("Installing blocker\n");
    let deferred = Promise.defer();
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

// Test that AsyncShutdown + OS.File reports errors correctly, in a case in which
// the latest operation succeeded

function setup_osfile_crash_noerror() {
  ChromeUtils.import("resource://gre/modules/Services.jsm", this);
  ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
  ChromeUtils.import("resource://gre/modules/Promise.jsm", this);

  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 1);
  Services.prefs.setBoolPref("toolkit.osfile.native", false);

  OS.File.profileBeforeChange.addBlocker(
    "Adding a blocker that will never be resolved",
    () => Promise.defer().promise
  );
  OS.File.getCurrentDirectory();

  Services.obs.notifyObservers(null, "profile-before-change");
  dump("Waiting for crash\n");
}

function after_osfile_crash_noerror(mdump, extra) {
  info("after OS.File crash: " + extra.AsyncShutdownTimeout);
  let data = JSON.parse(extra.AsyncShutdownTimeout);
  let state = data.conditions[0].state;
  info("Keys: " + Object.keys(state).join(", "));
  Assert.equal(data.phase, "profile-before-change");
  Assert.ok(state.launched);
  Assert.ok(!state.shutdown);
  Assert.ok(state.worker);
  Assert.ok(!!state.latestSent);
  Assert.equal(state.latestSent[1], "getCurrentDirectory");
}

// Test that AsyncShutdown + OS.File reports errors correctly, in a case in which
// the latest operation failed

function setup_osfile_crash_exn() {
  ChromeUtils.import("resource://gre/modules/Services.jsm", this);
  ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
  ChromeUtils.import("resource://gre/modules/Promise.jsm", this);

  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 1);
  Services.prefs.setBoolPref("toolkit.osfile.native", false);

  OS.File.profileBeforeChange.addBlocker(
    "Adding a blocker that will never be resolved",
    () => Promise.defer().promise
  );
  OS.File.read("I do not exist");

  Services.obs.notifyObservers(null, "profile-before-change");
  dump("Waiting for crash\n");
}

function after_osfile_crash_exn(mdump, extra) {
  info("after OS.File crash: " + extra.AsyncShutdownTimeout);
  let data = JSON.parse(extra.AsyncShutdownTimeout);
  let state = data.conditions[0].state;
  info("Keys: " + Object.keys(state).join(", "));
  Assert.equal(data.phase, "profile-before-change");
  Assert.ok(!state.shutdown);
  Assert.ok(state.worker);
  Assert.ok(!!state.latestSent);
  Assert.equal(state.latestSent[1], "read");
}

add_task(async function run_test() {
  await do_crash(setup_crash, after_crash);
  await do_crash(setup_osfile_crash_noerror, after_osfile_crash_noerror);
  await do_crash(setup_osfile_crash_exn, after_osfile_crash_exn);
});
