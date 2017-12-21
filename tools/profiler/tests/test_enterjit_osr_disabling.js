function run_test() {
    let p = Cc["@mozilla.org/tools/profiler;1"];
    // Just skip the test if the profiler component isn't present.
    if (!p)
      return;
    p = p.getService(Ci.nsIProfiler);
    if (!p)
      return;

    Assert.ok(!p.IsActive());

    p.StartProfiler(100, 10, ["js"], 1);
    // The function is entered with the profiler enabled
    (function() {
      p.StopProfiler();
      let n = 10000;
      while (--n); // OSR happens here with the profiler disabled.
      // An assertion will fail when this function returns, if the
      // profiler stack was misbalanced.
    })();
}
