// Test that we get `js::SavedStacks::saveCurrentStack` frames.

function run_test() {
  let p = Cc["@mozilla.org/tools/profiler;1"];
  // Just skip the test if the profiler component isn't present.
  if (!p)
    return;
  p = p.getService(Ci.nsIProfiler);
  if (!p)
    return;

  const { saveStack } = Cu.getJSTestingFunctions();

  const ms = 5;
  p.StartProfiler(100, ms, ["js"], 1);

  let then = Date.now();
  while (Date.now() - then < 30000) {
    function a() {
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
      saveStack();
    }

    a();
    a();
    a();
    a();
    a();

    function b() {
      a();
    }

    b();
    b();
    b();
    b();
    b();
  }

  var profile = p.getProfileData().threads[0];

  do_check_neq(profile.samples.data.length, 0);

  let found = false;
  for (let sample of profile.samples.data) {
    const stack = getInflatedStackLocations(profile, sample);
    for (let frame of stack) {
      if (frame.indexOf("js::SavedStacks::saveCurrentStack") >= 0) {
        found = true;
        break;
      }
    }
  }
  do_check_true(found);

  p.StopProfiler();
}
