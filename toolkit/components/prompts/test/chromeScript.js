Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Timer.jsm");

// Define these to make EventUtils happy.
let window = this;
let parent = {};

let EventUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);

addMessageListener("cancelPrompt", message => {
  cancelPromptWhenItAppears();
});

function cancelPromptWhenItAppears() {
  let interval = setInterval(() => {
    if (cancelPrompt()) {
      clearInterval(interval);
    }
  }, 100);
}

function cancelPrompt() {
  let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWin.gBrowser;
  let promptManager = gBrowser.getTabModalPromptBox(gBrowser.selectedBrowser);
  let prompts = promptManager.listPrompts();
  if (!prompts.length) {
    return false;
  }
  sendAsyncMessage("promptCanceled", {
    ui: {
      infoTitle: {
        hidden: prompts[0].ui.infoTitle.getAttribute("hidden") == "true",
      },
    },
  });
  EventUtils.synthesizeKey("KEY_Escape", { code: "Escape" }, browserWin);
  return true;
}

