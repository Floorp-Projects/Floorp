var setupModule = function(module) {
  controller = mozmill.getBrowserController();
}

/**
 * This test should fail and then exit with a 'Disconnect Error: Application Unexpectedly Closed'
 */
var testRestartAfterTimeout = function(){
  controller.startUserShutdown(1000, true);
  controller.sleep(2000);
  controller.window.Application.restart();
}
