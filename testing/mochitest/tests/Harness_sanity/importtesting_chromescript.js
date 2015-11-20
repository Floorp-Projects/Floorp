addMessageListener("ImportTesting:IsModuleLoaded", function (msg) {
  sendAsyncMessage("ImportTesting:IsModuleLoadedReply", Components.utils.isModuleLoaded(msg));
});
