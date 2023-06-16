function run_test() {
  Assert.ok(!Services.profiler.IsActive());

  // The function is entered with the profiler disabled.
  (function () {
    Services.profiler.StartProfiler(100, 10, ["js"]);
    let n = 10000;
    // eslint-disable-next-line no-empty
    while (--n) {} // OSR happens here with the profiler enabled.
    // An assertion will fail when this function returns, if the
    // profiler stack was misbalanced.
  })();
  Services.profiler.StopProfiler();
}
