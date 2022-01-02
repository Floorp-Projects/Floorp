/* import-globals-from crasher_subprocess_head.js */

// Let the event loop process a bit before crashing.
if (shouldDelay) {
  let shouldCrashNow = false;

  Services.tm.dispatchToMainThread({
    run: () => {
      shouldCrashNow = true;
    },
  });

  Services.tm.spinEventLoopUntil(
    "Test(crasher_subprocess_tail.js:shouldDelay)",
    () => shouldCrashNow
  );
}

// now actually crash
CrashTestUtils.crash(crashType);
