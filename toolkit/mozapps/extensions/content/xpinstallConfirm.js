// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var args

var XPInstallConfirm = {};

XPInstallConfirm.init = function XPInstallConfirm_init()
{
  var _installCountdown;
  var _installCountdownInterval;
  var _focused;
  var _timeout;

  var bundle = document.getElementById("xpinstallConfirmStrings");

  args = window.arguments[0].wrappedJSObject;

  var _installCountdownLength = 5;
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    var delay_in_milliseconds = prefs.getIntPref("security.dialog_enable_delay");
    _installCountdownLength = Math.round(delay_in_milliseconds / 500);
  } catch (ex) { }
  
  var itemList = document.getElementById("itemList");
  
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
    }
    else {
      installItem.cert = bundle.getString("unverified");
    }
    installItem.signed = install.certName ? "true" : "false";
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
    }
    else
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
    document.removeEventListener("focus", myfocus, true);
    document.removeEventListener("blur", myblur, true);
    window.removeEventListener("unload", myUnload, false);
  }

  if (_installCountdownLength > 0) {
    document.addEventListener("focus", myfocus, true);
    document.addEventListener("blur", myblur, true);
    window.addEventListener("unload", myUnload, false);

    okButton.disabled = true;
    setWidgetsAfterFocus();
  }
  else
    okButton.label = bundle.getString("installButtonLabel");
}

XPInstallConfirm.onOK = function XPInstallConfirm_onOk()
{
  Components.classes["@mozilla.org/base/telemetry;1"].
    getService(Components.interfaces.nsITelemetry).
    getHistogramById("SECURITY_UI").
    add(Components.interfaces.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL_CLICK_THROUGH);
  for (let install of args.installs)
    install.install();
  return true;
}

XPInstallConfirm.onCancel = function XPInstallConfirm_onCancel()
{
  for (let install of args.installs)
    install.cancel();
  return true;
}
