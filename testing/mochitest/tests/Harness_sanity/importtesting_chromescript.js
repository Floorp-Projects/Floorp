/* eslint-env mozilla/chrome-script */

addMessageListener("ImportTesting:IsModuleLoaded", function(msg) {
  sendAsyncMessage("ImportTesting:IsModuleLoadedReply", Cu.isModuleLoaded(msg));
});
