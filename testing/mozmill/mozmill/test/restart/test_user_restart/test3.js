var setupModule = function(module) {
  controller = mozmill.getBrowserController();
}

/**
 * This test should fail
 * (Restart expected but none detected before timeout)
 */
var testNoExpectedRestartByTimeout = function(){
  controller.startUserShutdown(1000, true);
  controller.sleep(2000);
}
