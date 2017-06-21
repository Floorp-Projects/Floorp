/* import-globals-from crasher_subprocess_head.js */

// Let the event loop process a bit before crashing.
if (shouldDelay) {
  let shouldCrashNow = false;
  let tm = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService(Components.interfaces.nsIThreadManager);

  tm.dispatchToMainThread({ run: () => { shouldCrashNow = true; } })

  tm.spinEventLoopUntil(() => shouldCrashNow);
}

// now actually crash
CrashTestUtils.crash(crashType);
