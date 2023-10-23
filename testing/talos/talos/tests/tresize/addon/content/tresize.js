/*
 * tresize-test - A purer form of paint measurement than tpaint. This
 * test opens a single window positioned at 10,10 and sized to 425,425,
 * then resizes the window outward |max| times measuring the amount of
 * time it takes to repaint each resize.  Calls the provided callback
 * with an array of resize times in milliseconds.
 */

async function runTest(callback, locationSearch) {
  const INCREMENT = 3;
  const MAX = 200;

  // Measure the time it take for the provided action to trigger a
  // MozAfterPaint event.  This function ensures that the event being
  // measured is really a result of the given action.
  function measurePaintTime(action, marker) {
    return new Promise(resolve => {
      let startTime;
      let lastTransaction = window.windowUtils.lastTransactionId;
      function painted(event) {
        if (event.transactionId <= lastTransaction) {
          return;
        }

        if (marker) {
          Profiler.subtestEnd(marker);
        }
        window.removeEventListener("MozAfterPaint", painted, true);
        let time = event.paintTimeStamp - startTime;
        resolve(time);
      }
      window.addEventListener("MozAfterPaint", painted, true);
      if (marker) {
        Profiler.subtestStart(marker);
      }
      startTime = window.performance.now();
      action();
    });
  }

  // Position the intial window and set up profiling...
  let windowSize = 425;
  await measurePaintTime(() => {
    window.moveTo(10, 10);
    window.resizeTo(windowSize, windowSize);
  });

  Profiler.initFromURLQueryParams(locationSearch);
  Profiler.beginTest("tresize");

  let times = [];
  for (let i = 0; i < MAX; i++) {
    const marker = `resize ${i}`;
    windowSize += INCREMENT;

    let time = await measurePaintTime(() => {
      window.resizeTo(windowSize, windowSize);
    }, marker);
    times.push(time);

    await new Promise(resolve => setTimeout(resolve, 20));
  }

  let total = times.reduce((a, b) => a + b);
  let average = total / times.length;
  callback({ average });
}
