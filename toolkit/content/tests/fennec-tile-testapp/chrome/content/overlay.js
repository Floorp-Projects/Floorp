var tile = {
  onLoad() {
    // initialization code
    this.initialized = true;
    this.strings = document.getElementById("tile-strings");
  },
  onMenuItemCommand(e) {
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    promptService.alert(window, this.strings.getString("helloMessageTitle"),
                                this.strings.getString("helloMessage"));
  },

};
window.addEventListener("load", function(e) { tile.onLoad(e); }, false);
