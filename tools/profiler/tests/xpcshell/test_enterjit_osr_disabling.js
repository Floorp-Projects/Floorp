function run_test() {
  // Just skip the test if the profiler component isn't present.
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  Assert.ok(!Services.profiler.IsActive());

  Services.profiler.StartProfiler(100, 10, ["js"]);
  // The function is entered with the profiler enabled
  (function() {
    Services.profiler.StopProfiler();
    let n = 10000;
    // eslint-disable-next-line no-empty
    while (--n) {} // OSR happens here with the profiler disabled.
    // An assertion will fail when this function returns, if the
    // profiler stack was misbalanced.
  })();
}
