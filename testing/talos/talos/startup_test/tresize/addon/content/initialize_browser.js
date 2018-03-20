function initializeBrowser(win) {
  Services.scriptloader.loadSubScript("chrome://tresize/content/Profiler.js", win);
  Services.scriptloader.loadSubScript("chrome://tresize/content/tresize.js", win);
  const TRESIZE_PREFIX = "tresize@mozilla.org:";

  var groupMM = win.getGroupMessageManager("browsers");
  groupMM.loadFrameScript("chrome://tresize/content/framescript.js", true);

  // listener/executor on the chrome process for tresize.html
  groupMM.addMessageListener(TRESIZE_PREFIX + "chrome-run-message", function listener(m) {
    function sendResult(result) {
      groupMM.broadcastAsyncMessage(TRESIZE_PREFIX + "chrome-run-reply", {
        id: m.data.id,
        result
      });
    }
    win.runTest(sendResult, m.data.locationSearch);
  });
}
