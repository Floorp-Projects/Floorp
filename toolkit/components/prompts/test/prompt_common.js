const Ci = SpecialPowers.Ci;
const Cc = SpecialPowers.Cc;
ok(Ci != null, "Access Ci");
ok(Cc != null, "Access Cc");

function hasTabModalPrompts() {
  var prefName = "prompts.tab_modal.enabled";
  var Services = SpecialPowers.Cu.import("resource://gre/modules/Services.jsm").Services;
  return Services.prefs.getPrefType(prefName) == Services.prefs.PREF_BOOL &&
         Services.prefs.getBoolPref(prefName);
}
var isTabModal = hasTabModalPrompts();
var isSelectDialog = false;
var isOSX = ("nsILocalFileMac" in SpecialPowers.Ci);
var isLinux = ("@mozilla.org/gnome-gconf-service;1" in SpecialPowers.Cc);
var isE10S = SpecialPowers.Services.appinfo.processType == 2;


var gChromeScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL("chromeScript.js"));
SimpleTest.registerCleanupFunction(() => gChromeScript.destroy());

function onloadPromiseFor(id) {
  var iframe = document.getElementById(id);
  return new Promise(resolve => {
    iframe.addEventListener("load", function onload(e) {
      iframe.removeEventListener("load", onload);
      resolve(true);
    });
  });
}

function handlePrompt(state, action) {
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

function checkEchoedAuthInfo(expectedState, doc) {
    // The server echos back the HTTP auth info it received.
    let username = doc.getElementById("user").textContent;
    let password = doc.getElementById("pass").textContent;
    let authok = doc.getElementById("ok").textContent;

    is(authok, "PASS", "Checking for successful authentication");
    is(username, expectedState.user, "Checking for echoed username");
    is(password, expectedState.pass, "Checking for echoed password");
}
