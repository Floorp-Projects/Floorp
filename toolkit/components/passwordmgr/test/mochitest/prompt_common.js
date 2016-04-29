var Ci = SpecialPowers.Ci;
ok(Ci != null, "Access Ci");
var Cc = SpecialPowers.Cc;
ok(Cc != null, "Access Cc");

var didDialog;

var timer; // keep in outer scope so it's not GC'd before firing
function startCallbackTimer() {
    didDialog = false;

    // Delay before the callback twiddles the prompt.
    const dialogDelay = 10;

    // Use a timer to invoke a callback to twiddle the authentication dialog
    timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.init(observer, dialogDelay, Ci.nsITimer.TYPE_ONE_SHOT);
}


var observer = SpecialPowers.wrapCallbackObject({
    QueryInterface : function (iid) {
        const interfaces = [Ci.nsIObserver,
                            Ci.nsISupports, Ci.nsISupportsWeakReference];

        if (!interfaces.some( function(v) { return iid.equals(v) } ))
            throw SpecialPowers.Components.results.NS_ERROR_NO_INTERFACE;
        return this;
    },

    observe : function (subject, topic, data) {
        var doc = getDialogDoc();
        if (doc)
            handleDialog(doc, testNum);
        else
            startCallbackTimer(); // try again in a bit
    }
});

function getDialogDoc() {
  // Find the <browser> which contains notifyWindow, by looking
  // through all the open windows and all the <browsers> in each.
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
    }
  }

  return null;
}

function getPromptState(ui) {
  let state = {};
  state.msg         = ui.infoBody.textContent;
  state.titleHidden = ui.infoTitle.getAttribute("hidden") == "true";
  state.textHidden  = ui.loginContainer.hidden;
  state.passHidden  = ui.password1Container.hidden;
  state.checkHidden = ui.checkboxContainer.hidden;
  state.checkMsg    = ui.checkbox.label;
  state.checked     = ui.checkbox.checked;
  // tab-modal prompts don't have an infoIcon
  state.iconClass   = ui.infoIcon ? ui.infoIcon.className : null;
  state.textValue   = ui.loginTextbox.getAttribute("value");
  state.passValue   = ui.password1Textbox.getAttribute("value");

  state.butt0Label  = ui.button0.label;
  state.butt1Label  = ui.button1.label;
  state.butt2Label  = ui.button2.label;

  state.butt0Disabled = ui.button0.disabled;
  state.butt1Disabled = ui.button1.disabled;
  state.butt2Disabled = ui.button2.disabled;

  function isDefaultButton(b) {
      return (b.hasAttribute("default") &&
              b.getAttribute("default") == "true");
  }
  state.defButton0 = isDefaultButton(ui.button0);
  state.defButton1 = isDefaultButton(ui.button1);
  state.defButton2 = isDefaultButton(ui.button2);

  let fm = Cc["@mozilla.org/focus-manager;1"].
           getService(Ci.nsIFocusManager);
  let e = fm.focusedElement;

  if (e == null) {
    state.focused = null;
  } else if (ui.button0.isSameNode(e)) {
    state.focused = "button0";
  } else if (ui.button1.isSameNode(e)) {
    state.focused = "button1";
  } else if (ui.button2.isSameNode(e)) {
    state.focused = "button2";
  } else if (ui.loginTextbox.inputField.isSameNode(e)) {
    state.focused = "textField";
  } else if (ui.password1Textbox.inputField.isSameNode(e)) {
    state.focused = "passField";
  } else if (ui.infoBody.isSameNode(e)) {
    state.focused = "infoBody";
  } else {
    state.focused = "ERROR: unexpected element focused: " + (e ? e.localName : "<null>");
  }

  return state;
}

var isTabModal = false;
var isOSX = ("nsILocalFileMac" in SpecialPowers.Ci);
var isLinux = ("@mozilla.org/gnome-gconf-service;1" in SpecialPowers.Cc);
var isE10S = SpecialPowers.Services.appinfo.processType == 2;

function checkPromptState(promptState, expectedState) {
    // XXX check title? OS X has title in content
    is(promptState.msg,         expectedState.msg,         "Checking expected message");
    if (isOSX && !isTabModal)
      ok(!promptState.titleHidden, "Checking title always visible on OS X");
    else
      is(promptState.titleHidden, expectedState.titleHidden, "Checking title visibility");
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

function onloadPromiseFor(id) {
  var iframe = document.getElementById(id);
  return new Promise(resolve => {
    iframe.addEventListener("load", function onload(e) {
      iframe.removeEventListener("load", onload);
      resolve(true);
    });
  });
}
