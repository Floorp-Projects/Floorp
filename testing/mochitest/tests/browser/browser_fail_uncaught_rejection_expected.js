setExpectedFailuresForSelfTest(1);

// The test will fail because there is only one of two expected rejections.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection."));
}
