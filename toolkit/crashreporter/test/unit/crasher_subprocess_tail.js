/* import-globals-from crasher_subprocess_head.js */

// Let the event loop process a bit before crashing.
if (shouldDelay) {
  let shouldCrashNow = false;

  Services.tm.dispatchToMainThread({
    run: () => {
      shouldCrashNow = true;
    },
  });

  Services.tm.spinEventLoopUntil(() => shouldCrashNow);
}

// now actually crash
CrashTestUtils.crash(crashType);
