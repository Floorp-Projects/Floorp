function run_test() {
    let p = Cc["@mozilla.org/tools/profiler;1"];
    // Just skip the test if the profiler component isn't present.
    if (!p)
	return;
    p = p.getService(Ci.nsIProfiler);
    if (!p)
	return;

    do_check_true(!p.IsActive());

    // The function is entered with the profiler disabled.
    (function() {
	p.StartProfiler(100, 10, ["js"], 1);
	let n = 10000;
	while (--n); // OSR happens here with the profiler enabled.
	// An assertion will fail when this function returns, if the
	// profiler stack was misbalanced.
    })();
    p.StopProfiler();
}
