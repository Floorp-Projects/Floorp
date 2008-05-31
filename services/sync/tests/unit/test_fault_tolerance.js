function run_test() {
  Components.utils.import("resource://weave/log4moz.js");
  Components.utils.import("resource://weave/faultTolerance.js");

  // Just make sure the getter works and the service is a singleton.
  FaultTolerance.Service._testProperty = "hi";
  do_check_eq(FaultTolerance.Service._testProperty, "hi");

  FaultTolerance.Service.init();

  var log = Log4Moz.Service.rootLogger;
  log.level = Log4Moz.Level.All;
  log.info("Testing.");
  do_check_eq(Log4Moz.Service.rootLogger.appenders.length, 1);
}
