function initializeBrowser(win) {
  Services.scriptloader.loadSubScript("chrome://damp/content/damp.js", win);

  const MESSAGE = "damp@mozilla.org:html-loaded";

  const groupMM = win.getGroupMessageManager("browsers");
  groupMM.loadFrameScript("chrome://damp/content/framescript.js", true);

  // Listen for damp.html load event before starting tests.
  // Each tppagecycles is going to reload damp.html and re-run DAMP entirely.
  groupMM.addMessageListener(MESSAGE, function onMessage(m) {
    (new win.Damp()).startTest();
  });
}
