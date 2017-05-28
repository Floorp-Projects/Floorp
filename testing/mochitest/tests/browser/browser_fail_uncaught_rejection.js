setExpectedFailuresForSelfTest(1);

function test() {
  Components.utils.import("resource://gre/modules/Promise.jsm", this);
  Promise.reject(new Error("Promise rejection."));
}
