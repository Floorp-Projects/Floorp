
function run_test() {
  // Check received messages

  var cps = Cc["@mozilla.org/content-pref/service;1"].
            createInstance(Ci.nsIContentPrefService).
            wrappedJSObject;

  var messageHandler = cps;
  // FIXME: For now, use the wrappedJSObject hack, until bug
  //        593407 which will clean that up. After that, use
  //        the commented out line below it.
  messageHandler = cps.wrappedJSObject;
  //messageHandler = cps.QueryInterface(Ci.nsIMessageListener);

  // Cannot get values
  do_check_false(messageHandler.receiveMessage({
    name: "ContentPref:getPref",
    json: { group: 'group2', name: 'name' } }).succeeded);

  // Cannot set general values
  messageHandler.receiveMessage({ name: "ContentPref:setPref",
    json: { group: 'group2', name: 'name', value: 'someValue' } });
  do_check_eq(cps.getPref('group', 'name', null), undefined);

  // Can set whitelisted values
  do_check_true(messageHandler.receiveMessage({ name: "ContentPref:setPref",
    json: { group: 'group2', name: 'browser.upload.lastDir',
            value: 'someValue' } }).succeeded);
  do_check_eq(cps.getPref('group2', 'browser.upload.lastDir', null), 'someValue');

  // Prepare for child tests

  // Manually listen to messages - the wakeup manager should do this
  // for us, but it doesn't run in xpcshell tests.
  var messageProxy = {
    receiveMessage: function(aMessage) {
      if (aMessage.name == 'ContentPref:QUIT') {
        // Undo mock storage.
        delete cps._mockStorage;
        delete cps._messageProxy;
        cps.setPref = cps.old_setPref;
        cps.getPref = cps.old_getPref;
        cps._dbInit = cps.old__dbInit;
        // Unlisten to messages
        mM.removeMessageListener("ContentPref:setPref", messageProxy);
        mM.removeMessageListener("ContentPref:getPref", messageProxy);
        mM.removeMessageListener("ContentPref:QUIT", messageProxy);
        do_test_finished();
        return true;
      } else {
        return messageHandler.receiveMessage(aMessage);
      }
    },
  };

  var mM = Cc["@mozilla.org/parentprocessmessagemanager;1"].
           getService(Ci.nsIMessageListenerManager);
  mM.addMessageListener("ContentPref:setPref", messageProxy);
  mM.addMessageListener("ContentPref:getPref", messageProxy);
  mM.addMessageListener("ContentPref:QUIT", messageProxy);

  // Mock storage. This is necessary because
  // the IPC xpcshell setup doesn't do well with the normal storage
  // engine.

  cps = cps.wrappedJSObject;
  cps._mockStorage = {};

  cps.old_setPref = cps.setPref;
  cps.setPref = function(aGroup, aName, aValue) {
    this._mockStorage[aGroup+':'+aName] = aValue;
  }

  cps.old_getPref = cps.getPref;
  cps.getPref = function(aGroup, aName) {
    return this._mockStorage[aGroup+':'+aName];
  }

  cps.old__dbInit = cps._dbInit;
  cps._dbInit = function(){};

  cps._messageProxy = messageProxy; // keep it alive
  do_test_pending();

  run_test_in_child("contentPrefs_childipc.js");
}

