setExpectedFailuresForSelfTest(3);

// Keep "JSMPromise" separate so "Promise" still refers to native Promises.
let JSMPromise = ChromeUtils.import("resource://gre/modules/Promise.jsm", {}).Promise;

function test() {
  Promise.reject(new Error("Promise rejection."));
  JSMPromise.reject(new Error("Promise.jsm rejection."));
  (async () => {
    throw new Error("Synchronous rejection from async function.");
  })();

  // The following rejections are caught, so they won't result in failures.
  Promise.reject(new Error("Promise rejection.")).catch(() => {});
  JSMPromise.reject(new Error("Promise.jsm rejection.")).catch(() => {});
  (async () => {
    throw new Error("Synchronous rejection from async function.");
  })().catch(() => {});
}
