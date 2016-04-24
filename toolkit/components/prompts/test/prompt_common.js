const Ci = SpecialPowers.Ci;
const Cc = SpecialPowers.Cc;
ok(Ci != null, "Access Ci");
ok(Cc != null, "Access Cc");

var didDialog;

var isSelectDialog = false;
var isTabModal = false;
if (SpecialPowers.getBoolPref("prompts.tab_modal.enabled")) {
    isTabModal = true;
}
var usePromptService = true;

var timer; // keep in outer scope so it's not GC'd before firing
function startCallbackTimer() {
    didDialog = false;

    // Delay before the callback twiddles the prompt.
    const dialogDelay = 10;

    // Use a timer to invoke a callback to twiddle the authentication dialog
    timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.init(observer, dialogDelay, Ci.nsITimer.TYPE_ONE_SHOT);
}


var observer = {
    QueryInterface : SpecialPowers.wrapCallback(function (iid) {
        const interfaces = [Ci.nsIObserver,
                            Ci.nsISupports, Ci.nsISupportsWeakReference];

        if (!interfaces.some( function(v) { return iid.equals(v) } ))
            throw SpecialPowers.Cr.NS_ERROR_NO_INTERFACE;
        return this;
    }),

    observe : SpecialPowers.wrapCallback(function (subject, topic, data) {
      try {
        if (isTabModal) {
          var promptBox = getTabModalPromptBox(window);
          ok(promptBox, "got tabmodal promptbox");
          var prompts = SpecialPowers.wrap(promptBox).listPrompts();
          if (prompts.length)
              handleDialog(prompts[0].Dialog.ui, testNum);
          else
              startCallbackTimer(); // try again in a bit
        } else {
          var doc = getDialogDoc();
          if (isSelectDialog && doc)
              handleDialog(doc, testNum);
          else if (doc)
              handleDialog(doc.defaultView.Dialog.ui, testNum);
          else
              startCallbackTimer(); // try again in a bit
        }
      } catch (e) {
        ok(false, "Exception thrown in the timer callback: " + e + " at " + (e.fileName || e.filename) + ":" + (e.lineNumber || e.linenumber));
      }
    })
};

function getTabModalPromptBox(domWin) {
    var promptBox = null;

    // Given a content DOM window, returns the chrome window it's in.
    function getChromeWindow(aWindow) {
        var chromeWin = SpecialPowers.wrap(aWindow).QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell)
                               .chromeEventHandler.ownerDocument.defaultView;
        return chromeWin;
    }

    try {
        // Get the topmost window, in case we're in a frame.
        var promptWin = domWin.top;

        // Get the chrome window for the content window we're using.
        var chromeWin = getChromeWindow(promptWin);

        if (chromeWin.getTabModalPromptBox)
            promptBox = chromeWin.getTabModalPromptBox(promptWin);
    } catch (e) {
        // If any errors happen, just assume no tabmodal prompter.
    }

    // Callers get confused by a wrapped promptBox here.
    return SpecialPowers.unwrap(promptBox);
}

function getDialogDoc() {
  // Trudge through all the open windows, until we find the one
  // that has either commonDialog.xul or selectDialog.xul loaded.
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  //var enumerator = wm.getEnumerator("navigator:browser");
  var enumerator = wm.getXULWindowEnumerator(null);

  while (enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    var windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;

    var containedDocShells = windowDocShell.getDocShellEnumerator(
                                      Ci.nsIDocShellTreeItem.typeChrome,
                                      Ci.nsIDocShell.ENUMERATE_FORWARDS);
    while (containedDocShells.hasMoreElements()) {
        // Get the corresponding document for this docshell
        var childDocShell = containedDocShells.getNext();
        // We don't want it if it's not done loading.
        if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
          continue;
        var childDoc = childDocShell.QueryInterface(Ci.nsIDocShell)
                                    .contentViewer
                                    .DOMDocument;

        //ok(true, "Got window: " + childDoc.location.href);
        if (childDoc.location.href == "chrome://global/content/commonDialog.xul")
          return childDoc;
        if (childDoc.location.href == "chrome://global/content/selectDialog.xul")
          return childDoc;
    }
  }

  return null;
}

var isOSX = ("nsILocalFileMac" in SpecialPowers.Ci);
var isLinux = ("@mozilla.org/gnome-gconf-service;1" in SpecialPowers.Cc);
var isE10S = SpecialPowers.Services.appinfo.processType == 2;

function handlePrompt() {
  return new Promise(resolve => {
    gChromeScript.addMessageListener("promptHandled", function handled(msg) {
      gChromeScript.removeMessageListener("promptHandled", handled);
      checkPromptState(msg.promptState, state);
      resolve(true);
    });
    gChromeScript.sendAsyncMessage("handlePrompt", { action: action, isTabModal: isTabModal});
  });
}

function checkPromptState(promptState, expectedState) {
    // XXX check title? OS X has title in content
    is(promptState.msg,         expectedState.msg,         "Checking expected message");
    is(promptState.textHidden,  expectedState.textHidden,  "Checking textbox visibility");
    is(promptState.passHidden,  expectedState.passHidden,  "Checking passbox visibility");
    is(promptState.checkHidden, expectedState.checkHidden, "Checking checkbox visibility");
    is(promptState.checkMsg,    expectedState.checkMsg,    "Checking checkbox label");
    is(promptState.checked,     expectedState.checked,     "Checking checkbox checked");
    if (!isTabModal)
      is(promptState.iconClass, "spaced " + expectedState.iconClass, "Checking expected icon CSS class");
    is(promptState.textValue, expectedState.textValue, "Checking textbox value");
    is(promptState.passValue, expectedState.passValue, "Checking passbox value");

    if (expectedState.butt0Label) {
        is(promptState.butt0Label, expectedState.butt0Label, "Checking accept-button label");
    }
    if (expectedState.butt1Label) {
        is(promptState.butt1Label, expectedState.butt1Label, "Checking cancel-button label");
    }
    if (expectedState.butt2Label) {
        is(promptState.butt2Label, expectedState.butt2Label, "Checking extra1-button label");
    }

    // For prompts with a time-delay button.
    if (expectedState.butt0Disabled) {
        is(promptState.butt0Disabled, true,  "Checking accept-button is disabled");
        is(promptState.butt1Disabled, false, "Checking cancel-button isn't disabled");
    }

    is(promptState.defButton0, expectedState.defButton == "button0", "checking button0 default");
    is(promptState.defButton1, expectedState.defButton == "button1", "checking button1 default");
    is(promptState.defButton2, expectedState.defButton == "button2", "checking button2 default");

    if (isLinux && (!promptState.focused || isE10S)) {
        todo(false, "Focus seems missing or wrong on Linux"); // bug 1265077
    } else if (isOSX && expectedState.focused && expectedState.focused.startsWith("button")) {
        is(promptState.focused, "infoBody", "buttons don't focus on OS X, but infoBody does instead");
    } else {
        is(promptState.focused, expectedState.focused, "Checking focused element");
    }
}
