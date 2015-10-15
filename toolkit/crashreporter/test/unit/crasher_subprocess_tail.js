// Let the event loop process a bit before crashing.
if (shouldDelay) {
  let shouldCrashNow = false;
  let thr = Components.classes["@mozilla.org/thread-manager;1"]
                          .getService().currentThread;
  thr.dispatch({ run: () => { shouldCrashNow = true; } },
               Components.interfaces.nsIThread.DISPATCH_NORMAL);

  while (!shouldCrashNow) {
    thr.processNextEvent(true);
  }
}

// now actually crash
CrashTestUtils.crash(crashType);
