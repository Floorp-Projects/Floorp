
function run_test() {
  let cps = Cc["@mozilla.org/content-pref/service;1"].
            createInstance(Ci.nsIContentPrefService);

  // Check received messages

  let messageHandler = cps;
  // FIXME: For now, use the wrappedJSObject hack, until bug
  //        593407 which will clean that up. After that, use
  //        the commented out line below it.
  messageHandler = cps.wrappedJSObject;
  //messageHandler = cps.QueryInterface(Ci.nsIFrameMessageListener);

  // Cannot get values
  do_check_false(messageHandler.receiveMessage({
    name: "ContentPref:getPref",
    json: { group: 'group2', name: 'name' } }).succeeded);

  // Cannot set general values
  messageHandler.receiveMessage({ name: "ContentPref:setPref",
    json: { group: 'group2', name: 'name', value: 'someValue' } });
  do_check_eq(cps.getPref('group', 'name'), undefined);

  // Can set whitelisted values
  do_check_true(messageHandler.receiveMessage({ name: "ContentPref:setPref",
    json: { group: 'group2', name: 'browser.upload.lastDir',
            value: 'someValue' } }).succeeded);
  do_check_eq(cps.getPref('group2', 'browser.upload.lastDir'), 'someValue');

  // Prepare for child tests

  // Mock construction for set/get for child. This is necessary because
  // the IPC xpcshell setup doens't do well with the normal storage
  // engine. But even with this mock interface, we are testing
  // the whitelisting functionality properly, which is all we need here.
  var mockStorage = {};
  cps.wrappedJSObject.setPref = function(aGroup, aName, aValue) {
    mockStorage[aGroup+':'+aName] = aValue;
  }
  cps.wrappedJSObject.getPref = function(aGroup, aName) {
    return mockStorage[aGroup+':'+aName];
  }

  // Manually listen to messages - the wakeup manager should do this
  // for us, but it doens't run in xpcshell tests
  var mM = Cc["@mozilla.org/parentprocessmessagemanager;1"].
           getService(Ci.nsIFrameMessageManager);
  mM.addMessageListener("ContentPref:setPref", messageHandler);
  mM.addMessageListener("ContentPref:getPref", messageHandler);

  run_test_in_child("test_contentPrefs_childipc.js");
}

