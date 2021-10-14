const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/Allowed rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection."));
  Promise.reject(new Error("Promise rejection."));
  Promise.reject(new Error("Allowed rejection."));
}
