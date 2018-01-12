// Check that the EnterJIT frame, added by the JIT trampoline and
// usable by a native unwinder to resume unwinding after encountering
// JIT code, is pushed as expected.
function run_test() {
    if (!AppConstants.MOZ_GECKO_PROFILER) {
      return;
    }

    // This test assumes that it's starting on an empty profiler stack.
    // (Note that the other profiler tests also assume the profiler
    // isn't already started.)
    Assert.ok(!Services.profiler.IsActive());

    const ms = 5;
    Services.profiler.StartProfiler(100, ms, ["js"], 1);

    function arbitrary_name() {
        // A frame for |arbitrary_name| has been pushed.  Do a sequence of
        // increasingly long spins until we get a sample.
        var delayMS = 5;
        while (1) {
            info("loop: ms = " + delayMS);
            let then = Date.now();
            do {
                let n = 10000;
                while (--n); // OSR happens here
                // Spin in the hope of getting a sample.
            } while (Date.now() - then < delayMS);
            let pr = Services.profiler.getProfileData().threads[0];
            if (pr.samples.data.length > 0 || delayMS > 30000)
                return pr;
            delayMS *= 2;
        }
    }

    var profile = arbitrary_name();

    Assert.notEqual(profile.samples.data.length, 0);
    var lastSample = profile.samples.data[profile.samples.data.length - 1];
    var stack = getInflatedStackLocations(profile, lastSample);
    info(stack);

    // All we can really check here is ensure that there is exactly
    // one arbitrary_name frame in the list.
    var gotName = false;
    for (var i = 0; i < stack.length; i++) {
        if (stack[i].match(/arbitrary_name/)) {
            Assert.equal(gotName, false);
            gotName = true;
        }
    }
    Assert.equal(gotName, true);

    Services.profiler.StopProfiler();
}
