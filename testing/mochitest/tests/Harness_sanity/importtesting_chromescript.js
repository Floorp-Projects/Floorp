addMessageListener("ImportTesting:IsModuleLoaded", function (msg) {
  sendAsyncMessage("ImportTesting:IsModuleLoadedReply", Cu.isModuleLoaded(msg));
});
