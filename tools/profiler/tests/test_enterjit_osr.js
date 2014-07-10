// Check that the EnterJIT frame, added by the JIT trampoline and
// usable by a native unwinder to resume unwinding after encountering
// JIT code, is pushed as expected.
function run_test() {
    let p = Cc["@mozilla.org/tools/profiler;1"];
    // Just skip the test if the profiler component isn't present.
    if (!p)
	return;
    p = p.getService(Ci.nsIProfiler);
    if (!p)
	return;

    // This test assumes that it's starting on an empty SPS stack.
    // (Note that the other profiler tests also assume the profiler
    // isn't already started.)
    do_check_true(!p.IsActive());

    const ms = 5;
    p.StartProfiler(100, ms, ["js"], 1);

    function arbitrary_name(){
        // A frame for |arbitrary_name| has been pushed.  Do a sequence of
        // increasingly long spins until we get a sample.
        var delayMS = 5;
        while (1) {
            do_print("loop: ms = " + delayMS);
	    let then = Date.now();
	    do {
	        let n = 10000;
	        while (--n); // OSR happens here
	        // Spin in the hope of getting a sample.
	    } while (Date.now() - then < delayMS);
            let pr = p.getProfileData().threads[0].samples;
            if (pr.length > 0 || delayMS > 30000)
                return pr;
            delayMS *= 2;
        }
    };

    var profile = arbitrary_name();

    do_check_neq(profile.length, 0);
    let stack = profile[profile.length - 1].frames.map(f => f.location);
    stack = stack.slice(stack.lastIndexOf("js::RunScript") + 1);

    do_print(stack);
    // This test needs to not break on platforms and configurations
    // where IonMonkey isn't available / enabled.
    if (stack.length < 2 || stack[1] != "EnterJIT") {
	do_print("No JIT?");
	// Try to check what we can....
	do_check_eq(Math.min(stack.length, 1), 1);
	let thisInterp = stack[0];
	do_check_eq(thisInterp.split(" ")[0], "arbitrary_name");
	if (stack.length >= 2) {
	    let nextFrame = stack[1];
	    do_check_neq(nextFrame.split(" ")[0], "arbitrary_name");
	}
    } else {
	do_check_eq(Math.min(stack.length, 3), 3);
	let thisInterp = stack[0];
	let enterJit = stack[1];
	let thisBC = stack[2];
	do_check_eq(thisInterp.split(" ")[0], "arbitrary_name");
	do_check_eq(enterJit, "EnterJIT");
	do_check_eq(thisBC.split(" ")[0], "arbitrary_name");
    }

    p.StopProfiler();
}
