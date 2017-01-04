// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var XPInstallConfirm = {};

XPInstallConfirm.init = function() {
  Components.utils.import("resource://gre/modules/AddonManager.jsm");

  var _installCountdown;
  var _installCountdownInterval;
  var _focused;
  var _timeout;

  // Default to cancelling the install when the window unloads
  XPInstallConfirm._installOK = false;

  var bundle = document.getElementById("xpinstallConfirmStrings");

  let args = window.arguments[0].wrappedJSObject;

  // If all installs have already been cancelled in some way then just close
  // the window
  if (args.installs.every(i => i.state != AddonManager.STATE_DOWNLOADED)) {
    window.close();
    return;
  }

  var _installCountdownLength = 5;
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    var delay_in_milliseconds = prefs.getIntPref("security.dialog_enable_delay");
    _installCountdownLength = Math.round(delay_in_milliseconds / 500);
  } catch (ex) { }

  var itemList = document.getElementById("itemList");

  let installMap = new WeakMap();
  let installListener = {
    onDownloadCancelled(install) {
      itemList.removeChild(installMap.get(install));
      if (--numItemsToInstall == 0)
        window.close();
    }
  };

  var numItemsToInstall = args.installs.length;
  for (let install of args.installs) {
    var installItem = document.createElement("installitem");
    itemList.appendChild(installItem);

    installItem.name = install.addon.name;
    installItem.url = install.sourceURI.spec;
    var icon = install.iconURL;
    if (icon)
      installItem.icon = icon;
    var type = install.type;
    if (type)
      installItem.type = type;
    if (install.certName) {
      installItem.cert = bundle.getFormattedString("signed", [install.certName]);
    } else {
      installItem.cert = bundle.getString("unverified");
    }
    installItem.signed = install.certName ? "true" : "false";

    installMap.set(install, installItem);
    install.addListener(installListener);
  }

  var introString = bundle.getString("itemWarnIntroSingle");
  if (numItemsToInstall > 4)
    introString = bundle.getFormattedString("itemWarnIntroMultiple", [numItemsToInstall]);
  var textNode = document.createTextNode(introString);
  var introNode = document.getElementById("itemWarningIntro");
  while (introNode.hasChildNodes())
    introNode.removeChild(introNode.firstChild);
  introNode.appendChild(textNode);

  var okButton = document.documentElement.getButton("accept");
  okButton.focus();

  function okButtonCountdown() {
    _installCountdown -= 1;

    if (_installCountdown < 1) {
      okButton.label = bundle.getString("installButtonLabel");
      okButton.disabled = false;
      clearInterval(_installCountdownInterval);
    } else
      okButton.label = bundle.getFormattedString("installButtonDisabledLabel", [_installCountdown]);
  }

  function myfocus() {
    // Clear the timeout if it exists so only the last one will be used.
    if (_timeout)
      clearTimeout(_timeout);

    // Use setTimeout since the last focus or blur event to fire is the one we
    // want
    _timeout = setTimeout(setWidgetsAfterFocus, 0);
  }

  function setWidgetsAfterFocus() {
    if (_focused)
      return;

    _installCountdown = _installCountdownLength;
    _installCountdownInterval = setInterval(okButtonCountdown, 500);
    okButton.label = bundle.getFormattedString("installButtonDisabledLabel", [_installCountdown]);
    _focused = true;
  }

  function myblur() {
    // Clear the timeout if it exists so only the last one will be used.
    if (_timeout)
      clearTimeout(_timeout);

    // Use setTimeout since the last focus or blur event to fire is the one we
    // want
    _timeout = setTimeout(setWidgetsAfterBlur, 0);
  }

  function setWidgetsAfterBlur() {
    if (!_focused)
      return;

    // Set _installCountdown to the inital value set in setWidgetsAfterFocus
    // plus 1 so when the window is focused there is immediate ui feedback.
    _installCountdown = _installCountdownLength + 1;
    okButton.label = bundle.getFormattedString("installButtonDisabledLabel", [_installCountdown]);
    okButton.disabled = true;
    clearInterval(_installCountdownInterval);
    _focused = false;
  }

  function myUnload() {
    if (_installCountdownLength > 0) {
      document.removeEventListener("focus", myfocus, true);
      document.removeEventListener("blur", myblur, true);
    }
    window.removeEventListener("unload", myUnload, false);

    for (let install of args.installs)
      install.removeListener(installListener);

    // Now perform the desired action - either install the
    // addons or cancel the installations
    if (XPInstallConfirm._installOK) {
      for (let install of args.installs)
        install.install();
    } else {
      for (let install of args.installs) {
        if (install.state != AddonManager.STATE_CANCELLED)
          install.cancel();
      }
    }
  }

  window.addEventListener("unload", myUnload, false);

  if (_installCountdownLength > 0) {
    document.addEventListener("focus", myfocus, true);
    document.addEventListener("blur", myblur, true);

    okButton.disabled = true;
    setWidgetsAfterFocus();
  } else
    okButton.label = bundle.getString("installButtonLabel");
}

XPInstallConfirm.onOK = function() {
  Components.classes["@mozilla.org/base/telemetry;1"].
    getService(Components.interfaces.nsITelemetry).
    getHistogramById("SECURITY_UI").
    add(Components.interfaces.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL_CLICK_THROUGH);
  // Perform the install or cancel after the window has unloaded
  XPInstallConfirm._installOK = true;
  return true;
}

XPInstallConfirm.onCancel = function() {
  // Perform the install or cancel after the window has unloaded
  XPInstallConfirm._installOK = false;
  return true;
}
