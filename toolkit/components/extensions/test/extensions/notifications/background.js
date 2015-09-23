browser.test.log("running background script");

var opts = {title: "Testing Notification", message: "Carry on"};

browser.notifications.create("5", opts, function(id){
  browser.test.sendMessage("running", id);
  browser.test.notifyPass("background test passed");
});

