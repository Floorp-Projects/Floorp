async function doGC() {
  // Run GC and CC a few times to make sure that as much as possible is freed.
  const numCycles = 3;
  for (let i = 0; i < numCycles; i++) {
    Cu.forceGC();
    Cu.forceCC();
    await new Promise(resolve => Cu.schedulePreciseShrinkingGC(resolve));
  }

  const MemoryReporter = Cc[
    "@mozilla.org/memory-reporter-manager;1"
  ].getService(Ci.nsIMemoryReporterManager);

  await new Promise(resolve => MemoryReporter.minimizeMemoryUsage(resolve));
}
