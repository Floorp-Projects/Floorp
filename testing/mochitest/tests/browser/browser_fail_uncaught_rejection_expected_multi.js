setExpectedFailuresForSelfTest(1);

// The test will fail because an expected uncaught rejection is actually caught.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection.")).catch(() => {});
}
