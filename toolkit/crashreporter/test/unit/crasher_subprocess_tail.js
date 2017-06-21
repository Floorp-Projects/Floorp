/* import-globals-from crasher_subprocess_head.js */

// Let the event loop process a bit before crashing.
if (shouldDelay) {
  let shouldCrashNow = false;
  let tm = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService();
  let thr = tm.currentThread;
  thr.dispatch({ run: () => { shouldCrashNow = true; } },
               Components.interfaces.nsIThread.DISPATCH_NORMAL);

  tm.spinEventLoopUntil(() => shouldCrashNow);
}

// now actually crash
CrashTestUtils.crash(crashType);
