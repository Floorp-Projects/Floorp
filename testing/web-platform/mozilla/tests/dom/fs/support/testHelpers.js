async function waitUntil(isWaitDone, untilMs, stepMs = 25) {
  const startMs = Date.now();

  return new Promise((resolve, reject) => {
      const areWeDoneYet = setInterval(async function() {
        if (await isWaitDone()) {
          clearInterval(areWeDoneYet);
          resolve();
        } else if (Date.now() > startMs + untilMs) {
          clearInterval(areWeDoneYet);
          reject(new Error("Timed out after " + untilMs + "ms"));
        }
      }, stepMs);
  });
}
