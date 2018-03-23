function initializeBrowser(win) {
  Services.scriptloader.loadSubScript("chrome://tart/content/Profiler.js", win);
  Services.scriptloader.loadSubScript("chrome://tart/content/tart.js", win);
  var prefs = Services.prefs;

  const TART_PREFIX = "tart@mozilla.org:";

  // "services" which the framescript can execute at the chrome process
  var proxiedServices = {
    runTest(config, callback) {
      (new win.Tart()).startTest(callback, config);
    },

    setASAP() {
      prefs.setIntPref("layout.frame_rate", 0);
      prefs.setIntPref("docshell.event_starvation_delay_hint", 1);
    },

    unsetASAP() {
      prefs.clearUserPref("layout.frame_rate");
      prefs.clearUserPref("docshell.event_starvation_delay_hint");
    },

    toClipboard(text) {
      const gClipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"]
                               .getService(Ci.nsIClipboardHelper);
      gClipboardHelper.copyString(text);
    }
  };

  var groupMM = win.getGroupMessageManager("browsers");
  groupMM.loadFrameScript("chrome://tart/content/framescript.js", true);

  // listener/executor on the chrome process for tart.html
  groupMM.addMessageListener(TART_PREFIX + "chrome-exec-message", function listener(m) {
    function sendResult(result) {
      groupMM.broadcastAsyncMessage(TART_PREFIX + "chrome-exec-reply", {
        id: m.data.id,
        result
      });
    }

    var command = m.data.command;
    if (!proxiedServices.hasOwnProperty(command.name))
      throw new Error("TART: service doesn't exist: '" + command.name + "'");

    var service = proxiedServices[command.name];
    if (command.name == "runTest") // Needs async execution
      service(command.data, sendResult);
    else
      sendResult(service(command.data));

  });
}
