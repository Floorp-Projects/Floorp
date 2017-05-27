setExpectedFailuresForSelfTest(1);

// The test will fail because an expected uncaught rejection is actually caught.
Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection.")).catch(() => {});
}
