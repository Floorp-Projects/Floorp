setExpectedFailuresForSelfTest(1);

// The test will fail because an expected uncaught rejection is actually caught.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection.")).catch(() => {});
}
