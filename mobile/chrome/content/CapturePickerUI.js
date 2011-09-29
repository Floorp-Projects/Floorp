var CapturePickerUI = {
  init: function() {
    this.messageManager = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
    this.messageManager.addMessageListener("CapturePicker:Show", this);
  },
  
  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "CapturePicker:Show":
        let params = { result: true };
        let dialog = importDialog(null, "chrome://browser/content/CaptureDialog.xul", params);
        document.getElementById("capturepicker-title").appendChild(document.createTextNode(aMessage.json.title));
        dialog.waitForClose();
        return { value: params.result, path: params.path };
        break;
    }
    // prevents warning from the script loader
    return null;
  }
};
