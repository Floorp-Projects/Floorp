# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is Ben Goodger. 
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com> (v2.0) 
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

var args

var XPInstallConfirm = {};

XPInstallConfirm.init = function ()
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
  for (var i = 0; i < numItemsToInstall; ++i) {
    var installItem = document.createElement("installitem");
    itemList.appendChild(installItem);

    installItem.name = args.installs[i].addon.name;
    installItem.url = args.installs[i].sourceURI.spec;
    var icon = args.installs[i].iconURL;
    if (icon)
      installItem.icon = icon;
    var type = args.installs[i].type;
    if (type)
      installItem.type = type;
    if (args.installs[i].certName) {
      installItem.cert = bundle.getFormattedString("signed", [args.installs[i].certName]);
    }
    else {
      installItem.cert = bundle.getString("unverified");
    }
    installItem.signed = args.installs[i].certName ? "true" : "false";
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

XPInstallConfirm.onOK = function ()
{
  args.installs.forEach(function(install) {
    install.install();
  });
  return true;
}

XPInstallConfirm.onCancel = function ()
{
  args.installs.forEach(function(install) {
    install.cancel();
  });
  return true;
}
