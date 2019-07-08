/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function closeWindow(aClose, aPromptFunction) {
  let { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
  );

  // Closing the last window doesn't quit the application on OS X.
  if (AppConstants.platform != "macosx") {
    var windowCount = 0;
    for (let w of Services.wm.getEnumerator(null)) {
      if (w.closed) {
        continue;
      }
      if (++windowCount == 2) {
        break;
      }
    }

    // If we're down to the last window and someone tries to shut down, check to make sure we can!
    if (windowCount == 1 && !canQuitApplication("lastwindow")) {
      return false;
    }
    if (
      windowCount != 1 &&
      typeof aPromptFunction == "function" &&
      !aPromptFunction()
    ) {
      return false;
    }
  } else if (typeof aPromptFunction == "function" && !aPromptFunction()) {
    return false;
  }

  if (aClose) {
    window.close();
    return window.closed;
  }

  return true;
}

function canQuitApplication(aData) {
  try {
    var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      aData || null
    );

    // Something aborted the quit process.
    if (cancelQuit.data) {
      return false;
    }
  } catch (ex) {}
  return true;
}

function goQuitApplication() {
  if (!canQuitApplication()) {
    return false;
  }

  Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
  return true;
}

//
// Command Updater functions
//
function goUpdateCommand(aCommand) {
  try {
    var controller = top.document.commandDispatcher.getControllerForCommand(
      aCommand
    );

    var enabled = false;
    if (controller) {
      enabled = controller.isCommandEnabled(aCommand);
    }

    goSetCommandEnabled(aCommand, enabled);
  } catch (e) {
    Cu.reportError(
      "An error occurred updating the " + aCommand + " command: " + e
    );
  }
}

function goDoCommand(aCommand) {
  try {
    var controller = top.document.commandDispatcher.getControllerForCommand(
      aCommand
    );
    if (controller && controller.isCommandEnabled(aCommand)) {
      controller.doCommand(aCommand);
    }
  } catch (e) {
    Cu.reportError(
      "An error occurred executing the " + aCommand + " command: " + e
    );
  }
}

function goSetCommandEnabled(aID, aEnabled) {
  var node = document.getElementById(aID);

  if (node) {
    if (aEnabled) {
      node.removeAttribute("disabled");
    } else {
      node.setAttribute("disabled", "true");
    }
  }
}
