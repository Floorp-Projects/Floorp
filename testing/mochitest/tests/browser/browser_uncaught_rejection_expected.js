const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/Allowed rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection."));
  Promise.reject(new Error("Promise rejection."));
  Promise.reject(new Error("Allowed rejection."));
}
