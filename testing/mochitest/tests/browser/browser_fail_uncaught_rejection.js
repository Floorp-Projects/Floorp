setExpectedFailuresForSelfTest(2);

function test() {
  Promise.reject(new Error("Promise rejection."));
  (async () => {
    throw new Error("Synchronous rejection from async function.");
  })();

  // The following rejections are caught, so they won't result in failures.
  Promise.reject(new Error("Promise rejection.")).catch(() => {});
  (async () => {
    throw new Error("Synchronous rejection from async function.");
  })().catch(() => {});
}
