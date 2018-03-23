function initializeBrowser(win) {
  Services.scriptloader.loadSubScript("chrome://talos-powers-content/content/TalosParentProfiler.js", win);
  Services.scriptloader.loadSubScript("chrome://damp/content/damp.js", win);

  const PREFIX = "damp@mozilla.org:";

  // "services" which the framescript can execute at the chrome process
  var proxiedServices = {
    runTest(config, callback) {
      (new win.Damp()).startTest(callback, config);
    },

    toClipboard(text) {
      const gClipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"]
                                   .getService(Ci.nsIClipboardHelper);
      gClipboardHelper.copyString(text);
    }
  };

  var groupMM = win.getGroupMessageManager("browsers");
  groupMM.loadFrameScript("chrome://damp/content/framescript.js", true);

  // listener/executor on the chrome process for damp.html
  groupMM.addMessageListener(PREFIX + "chrome-exec-message", function listener(m) {
    function sendResult(result) {
      groupMM.broadcastAsyncMessage(PREFIX + "chrome-exec-reply", {
        id: m.data.id,
        result
      });
    }

    var command = m.data.command;
    if (!proxiedServices.hasOwnProperty(command.name))
      throw new Error("DAMP: service doesn't exist: '" + command.name + "'");

    var service = proxiedServices[command.name];
    if (command.name == "runTest") // Needs async execution
      service(command.data, sendResult);
    else
      sendResult(service(command.data));

  });
}
