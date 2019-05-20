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
    Services.profiler.StartProfiler(10000, ms, ["js"], 1);

    function has_arbitrary_name_in_stack() {
        // A frame for |arbitrary_name| has been pushed.  Do a sequence of
        // increasingly long spins until we get a sample.
        var delayMS = 5;
        while (1) {
            info("loop: ms = " + delayMS);
            const then = Date.now();
            do {
                let n = 10000;
                while (--n); // OSR happens here
                // Spin in the hope of getting a sample.
            } while (Date.now() - then < delayMS);
            let profile = Services.profiler.getProfileData().threads[0];

            // Go through all of the stacks, and search for this function name.
            for (const sample of profile.samples.data) {
                const stack = getInflatedStackLocations(profile, sample);
                info(`The following stack was found: ${stack}`);
                for (var i = 0; i < stack.length; i++) {
                    if (stack[i].match(/arbitrary_name/)) {
                        // This JS sample was correctly found.
                        return true;
                    }
                }
            }

            // Continue running this function with an increasingly long delay.
            delayMS *= 2;
            if (delayMS > 30000) {
                return false;
            }
        }
    }
    Assert.ok(has_arbitrary_name_in_stack(), "A JS frame was found before the test timeout.");
    Services.profiler.StopProfiler();
}
