setExpectedFailuresForSelfTest(1);

// The test will fail because there is only one of two expected rejections.
Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);
PromiseTestUtils.expectUncaughtRejection(/Promise rejection./);

function test() {
  Promise.reject(new Error("Promise rejection."));
}
