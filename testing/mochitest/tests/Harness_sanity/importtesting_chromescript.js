/* eslint-env mozilla/frame-script */

addMessageListener("ImportTesting:IsModuleLoaded", function(msg) {
  sendAsyncMessage("ImportTesting:IsModuleLoadedReply", Cu.isModuleLoaded(msg));
});
