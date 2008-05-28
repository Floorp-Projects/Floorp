function run_test() {
  Components.utils.import("resource://weave/faultTolerance.js");

  // Just make sure the getter works and the service is a singleton.
  FaultTolerance.Service._testProperty = "hi";
  do_check_eq(FaultTolerance.Service._testProperty, "hi");
}
