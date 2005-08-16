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

var XPInstallConfirm = 
{ 
  _param: null
};


XPInstallConfirm.init = function ()
{
  var _installCountdown = 2;
  var _installCountdownInterval = -1;

  var bundle = document.getElementById("xpinstallConfirmStrings");
  
  this._param = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
  if (!this._param)
    close();
  
  this._param.SetInt(0, 1); // The default return value is "Cancel"
  
  var itemList = document.getElementById("itemList");
  
  var numItemsToInstall = this._param.GetInt(1);
  for (var i = 0; i < numItemsToInstall; ++i) {
    var installItem = document.createElement("installitem");
    itemList.appendChild(installItem);

    installItem.name = this._param.GetString(i);
    installItem.url = this._param.GetString(++i);
    var icon = this._param.GetString(++i);
    if (icon != "")
      installItem.icon = icon;
    var cert = this._param.GetString(++i);
    installItem.cert = cert || bundle.getString("Unsigned");
    installItem.signed = cert ? "true" : "false";
  }
  
  var introString = bundle.getString("itemWarningIntroSingle");
  if (numItemsToInstall > 4)
    introString = bundle.getFormattedString("itemWarningIntroMultiple", [numItemsToInstall / 4]);
  if (this._param.objects && this._param.objects.length)
    introString = this._param.objects.queryElementAt(0, Components.interfaces.nsISupportsString).data;
  var textNode = document.createTextNode(introString);
  var introNode = document.getElementById("itemWarningIntro");
  while (introNode.hasChildNodes())
    introNode.removeChild(introNode.firstChild);
  introNode.appendChild(textNode);
  
  var okButton = document.documentElement.getButton("accept");
  okButton.label = bundle.getFormattedString("installButtonDisabledLabel", [_installCountdown.toFixed(1)]);
  okButton.disabled = true;
  okButton.focus();

  function okButtonCountdown() {
    _installCountdown -= 0.1;

    if (_installCountdown <= 0) {
      okButton.label = bundle.getString("installButtonLabel");
      okButton.disabled = false;
      clearInterval(_installCountdownInterval);
      _installCountdownInterval = -1;
    }
    else
      okButton.label = bundle.getFormattedString("installButtonDisabledLabel", [_installCountdown.toFixed(1)]);
  }

  function myfocus(event) {
    if (_installCountdownInterval == -1)
      _installCountdownInterval = setInterval(okButtonCountdown, 100);

    // When the dialog is focused, we get *three* focus events, two targeted
    // at the document itself and one at the internal XUL element. Only reset
    // the counter if the document itself is the target.
    if (event.target == document) {
      _installCountdown = 2;
      okButton.label = bundle.getFormattedString("installButtonDisabledLabel", [_installCountdown.toFixed(1)]);
      okButton.disabled = true;
    }
  }

  function myblur() { 
    // When the dialog is blurred, we only get one blur event, targeted
    // a the currently focused XUL element. We cannot distinguish between
    // an internal focus change and a window change. Stop the countdown, but
    // don't disable.
    if (_installCountdownInterval != -1) {
      clearInterval(_installCountdownInterval);
      _installCountdownInterval = -1;
    }
  }

  document.addEventListener("focus", myfocus, true);
  document.addEventListener("blur", myblur, true);

  _installCountdownInterval = setInterval(okButtonCountdown, 100);
}

XPInstallConfirm.onOK = function ()
{
  this._param.SetInt(0, 0);
  return true;
}

XPInstallConfirm.onCancel = function ()
{
  this._param.SetInt(0, 1);
  return true;
}
