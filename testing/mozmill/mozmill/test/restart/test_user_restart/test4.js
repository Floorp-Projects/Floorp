var setupModule = function(module) {
  controller = mozmill.getBrowserController();
}

/**
 * This test should fail
 * (Restart expected but none detected before end of test)
 */
var testNoExpectedRestartByEndTest = function(){
  controller.startUserShutdown(1000, true);
  controller.sleep(100);
}
