ChromeUtils.import("resource://gre/modules/ProfileAge.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://services-common/utils.js");

const gProfD = do_get_profile();

// Creates a unique profile directory to use for a test.
function withDummyProfile(task) {
  return async () => {
    let profile = OS.Path.join(gProfD.path, "" + Math.floor(Math.random() * 100));
    await OS.File.makeDir(profile);
    await task(profile);
    await OS.File.removeDir(profile);
  };
}

add_task(withDummyProfile(async (profile) => {
  let times = await ProfileAge(profile);
  Assert.ok((await times.created) > 0, "We can't really say what this will be, just assume if it is a number it's ok.");
  Assert.equal(await times.reset, undefined, "Reset time is undefined in a new profile");
}));

add_task(withDummyProfile(async (profile) => {
  const CREATED_TIME = Date.now() - 2000;
  const RESET_TIME = Date.now() - 1000;

  CommonUtils.writeJSON({
    created: CREATED_TIME,
  }, OS.Path.join(profile, "times.json"));

  let times = await ProfileAge(profile);
  Assert.equal((await times.created), CREATED_TIME, "Should have seen the right profile time.");

  let times2 = await ProfileAge(profile);
  Assert.equal(times, times2, "Should have got the same instance.");

  let promise = times.recordProfileReset(RESET_TIME);
  Assert.equal((await times2.reset), RESET_TIME, "Should have seen the right reset time in the second instance immediately.");
  await promise;

  let results = await CommonUtils.readJSON(OS.Path.join(profile, "times.json"));
  Assert.deepEqual(results, {
    created: CREATED_TIME,
    reset: RESET_TIME,
  }, "Should have seen the right results.");
}));

add_task(withDummyProfile(async (profile) => {
  const RESET_TIME = Date.now() - 1000;
  const RESET_TIME2 = Date.now() - 2000;

  // The last call to recordProfileReset should always win.
  let times = await ProfileAge(profile);
  await Promise.all([
    times.recordProfileReset(RESET_TIME),
    times.recordProfileReset(RESET_TIME2),
  ]);

  let results = await CommonUtils.readJSON(OS.Path.join(profile, "times.json"));
  Assert.deepEqual(results, {
    reset: RESET_TIME2,
  }, "Should have seen the right results.");
}));
