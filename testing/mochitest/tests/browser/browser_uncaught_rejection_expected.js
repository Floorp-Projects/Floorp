// Keep "JSMPromise" separate so "Promise" still refers to native Promises.
let JSMPromise = ChromeUtils.import("resource://gre/modules/Promise.jsm", {})
  .Promise;

ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.allowRejectionsGlobally(/Whitelisted rejection./);
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
