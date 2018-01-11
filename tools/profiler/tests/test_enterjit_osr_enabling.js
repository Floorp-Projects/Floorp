function run_test() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  Assert.ok(!Services.profiler.IsActive());

  // The function is entered with the profiler disabled.
  (function() {
    Services.profiler.StartProfiler(100, 10, ["js"], 1);
    let n = 10000;
    while (--n); // OSR happens here with the profiler enabled.
    // An assertion will fail when this function returns, if the
    // profiler stack was misbalanced.
  })();
  Services.profiler.StopProfiler();
}
