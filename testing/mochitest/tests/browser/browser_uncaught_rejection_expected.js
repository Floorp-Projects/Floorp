// Keep "JSMPromise" separate so "Promise" still refers to native Promises.
let JSMPromise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.whitelistRejectionsGlobally(/Whitelisted rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise.jsm rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise.jsm rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection."));
  Promise.reject(new Error("Promise rejection."));
  Promise.reject(new Error("Whitelisted rejection."));
  JSMPromise.reject(new Error("Promise.jsm rejection."));
  JSMPromise.reject(new Error("Promise.jsm rejection."));
  JSMPromise.reject(new Error("Whitelisted rejection."));
}
