/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice.desre@gmail.com>, Original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function promptService() {
  let bundleService = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
  this._bundle = bundleService.createBundle("chrome://global/locale/commonDialogs.properties");
}

promptService.prototype = {
  classDescription: "Mobile Prompt Service",
  contractID: "@mozilla.org/embedcomp/prompt-service;1",
  classID: Components.ID("{9a61149b-2276-4a0a-b79c-be994ad106cf}"),
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptService, Ci.nsIPromptService2]),
 
  // helper function do get the current document
  getDocument: function() {
    let wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
    let win = wm.getMostRecentWindow("navigator:browser");
    return win ? win.document : null;
  },
 
  // add a width style to prevent a element to grow larger 
  // than the screen width
  sizeElement: function(id, percent) {
    let elem = this.getDocument().getElementById(id);
    let screenW = this.getDocument().getElementById("main-window").getBoundingClientRect().width;
    elem.style.width = screenW * percent / 100 + "px"
  },
  
  // size the height of the scrollable message. this assumes the given element
  // is a child of a scrollbox
  sizeScrollableMsg: function(id, percent) {
    let doc = this.getDocument();
    let screenH = doc.getElementById("main-window").getBoundingClientRect().height;
    let maxHeight = screenH * percent / 100;
    
    let elem = doc.getElementById(id);
    let style = doc.defaultView.getComputedStyle(elem, null);
    let height = Math.ceil(elem.getBoundingClientRect().height) +
                 parseInt(style.marginTop) +
                 parseInt(style.marginBottom);

    if (height > maxHeight)
      height = maxHeight;
    elem.parentNode.style.height = height + "px";
  },
  
  openDialog: function(aParent, aSrc, aParams) {
    let wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
    let browser = wm.getMostRecentWindow("navigator:browser");
    return browser.importDialog(aParent, aSrc, aParams);
  },
  
  _getFallbackService: function() {
    return Components.classesByID["{a2112d6a-0e28-421f-b46a-25c0B308cbd0}"]
                     .getService(Ci.nsIPromptService);
  },
  
  alert: function(aParent, aTitle, aText) {
    let doc = this.getDocument();
    if (!doc) {
      this._getFallbackService().alert(aParent, aTitle, aText);
      return;
    }
    
    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/alert.xul", null);
    doc.getElementById("prompt-alert-title").value = aTitle;
    doc.getElementById("prompt-alert-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-alert-message", 80);
    this.sizeScrollableMsg("prompt-alert-message", 25);
    
    dialog.waitForClose();
  },
  
  alertCheck: function(aParent, aTitle, aText, aCheckMsg, aCheckState) {
    let doc = this.getDocument();
    if (!doc) {
      this._getFallbackService().alertCheck(aParent, aTitle, aText, aCheckMsg, aCheckState);
      return;
    }
    
    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/alert.xul", aCheckState);
    doc.getElementById("prompt-alert-title").value = aTitle;
    doc.getElementById("prompt-alert-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-alert-message", 80);
    this.sizeScrollableMsg("prompt-alert-message", 25);
    
    doc.getElementById("prompt-alert-checkbox").checked = aCheckState.value;
    this.setLabelForNode(doc.getElementById("prompt-alert-checkbox-msg"), aCheckMsg);
    this.sizeElement("prompt-alert-checkbox-msg", 50);
    doc.getElementById("prompt-alert-checkbox-box").removeAttribute("collapsed");
    
    dialog.waitForClose();
  },
  
  confirm: function(aParent, aTitle, aText) {
    let doc = this.getDocument();
    if (!doc) {
      return this._getFallbackService().confirm(aParent, aTitle, aText);
    }

    var params = new Object();
    params.result = false;

    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/confirm.xul", params);
    doc.getElementById("prompt-confirm-title").value = aTitle;
    doc.getElementById("prompt-confirm-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-confirm-message", 80);
    this.sizeScrollableMsg("prompt-confirm-message", 25);
    
    dialog.waitForClose();
    return params.result;
  },
  
  confirmCheck: function(aParent, aTitle, aText, aCheckMsg, aCheckState) {
    let doc = this.getDocument();
    if (!doc) {
      return this._getFallbackService().confirmCheck(aParent, aTitle, aText, aCheckMsg, aCheckState);
    }

    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;

    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/confirm.xul", params);
    doc.getElementById("prompt-confirm-title").value = aTitle;
    doc.getElementById("prompt-confirm-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-confirm-message", 80);
    this.sizeScrollableMsg("prompt-confirm-message", 25);

    doc.getElementById("prompt-confirm-checkbox").checked = aCheckState.value;
    this.setLabelForNode(doc.getElementById("prompt-confirm-checkbox-msg"), aCheckMsg);
    this.sizeElement("prompt-confirm-checkbox-msg", 50);
    doc.getElementById("prompt-confirm-checkbox-box").removeAttribute("collapsed");
    
    dialog.waitForClose();
    return params.result;
  },
  
  getLocaleString: function(key) {
    return this._bundle.GetStringFromName(key);
  },
  
  //
  // Copied from chrome://global/content/commonDialog.js
  //
  setLabelForNode: function(aNode, aLabel) {
    // This is for labels which may contain embedded access keys.
    // If we end in (&X) where X represents the access key, optionally preceded
    // by spaces and/or followed by the ':' character, store the access key and
    // remove the access key placeholder + leading spaces from the label.
    // Otherwise a character preceded by one but not two &s is the access key.
    // Store it and remove the &.
  
    // Note that if you change the following code, see the comment of
    // nsTextBoxFrame::UpdateAccessTitle.
 
    if (!aLabel)
      return;

    var accessKey = null;
    if (/ *\(\&([^&])\)(:)?$/.test(aLabel)) {
      aLabel = RegExp.leftContext + RegExp.$2;
      accessKey = RegExp.$1;
    } else if (/^(.*[^&])?\&(([^&]).*$)/.test(aLabel)) {
      aLabel = RegExp.$1 + RegExp.$2;
      accessKey = RegExp.$3;
    }
  
    // && is the magic sequence to embed an & in your label.
    aLabel = aLabel.replace(/\&\&/g, "&");
    if (aNode instanceof Ci.nsIDOMXULLabelElement) {
      aNode.setAttribute("value", aLabel);
    } else if (aNode instanceof Ci.nsIDOMXULDescriptionElement) {
      let text = aNode.ownerDocument.createTextNode(aLabel);
      aNode.appendChild(text);
    } else {    // Set text for other xul elements
      aNode.setAttribute("label", aLabel);
    }
    
    // XXXjag bug 325251
    // Need to set this after aNode.setAttribute("value", aLabel);
    if (accessKey)
      aNode.setAttribute("accesskey", accessKey);
  },

  confirmEx: function(aParent, aTitle, aText, aButtonFlags, aButton0,
                      aButton1, aButton2, aCheckMsg, aCheckState) {
    let doc = this.getDocument();
    if (!doc) {
      return this._getFallbackService().confirmEx(aParent, aTitle, aText, aButtonFlags, aButton0,
            aButton1, aButton2, aCheckMsg, aCheckState);
    }

    let numButtons = 0;
    let titles = [aButton0, aButton1, aButton2];
    
    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;

    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/confirm.xul", params);
    doc.getElementById("prompt-confirm-title").value = aTitle;
    doc.getElementById("prompt-confirm-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-confirm-message", 80);
    this.sizeScrollableMsg("prompt-confirm-message", 25);

    doc.getElementById("prompt-confirm-checkbox").checked = aCheckState.value;
    this.setLabelForNode(doc.getElementById("prompt-confirm-checkbox-msg"), aCheckMsg);
    this.sizeElement("prompt-confirm-checkbox-msg", 50);
    if (aCheckMsg) {
      doc.getElementById("prompt-confirm-checkbox-box").removeAttribute("collapsed");
    }
    
    let bbox = doc.getElementById("prompt-confirm-button-box");
    while (bbox.lastChild) {
      bbox.removeChild(bbox.lastChild);
    }
    
    for (let i = 0; i < 3; i++) {
      let bTitle = null;
      switch (aButtonFlags & 0xff) {
        case Ci.nsIPromptService.BUTTON_TITLE_OK :
          bTitle = this.getLocaleString("OK");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_CANCEL :
          bTitle = this.getLocaleString("Cancel");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_YES :
          bTitle = this.getLocaleString("Yes");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_NO :
          bTitle = this.getLocaleString("No");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_SAVE :
          bTitle = this.getLocaleString("Save");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_DONT_SAVE :
          bTitle = this.getLocaleString("DontSave");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_REVERT :
          bTitle = this.getLocaleString("Revert");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_IS_STRING :
          bTitle = titles[i];
        break;
      }
      
      if (bTitle) {
        let button = doc.createElement("button");
        this.setLabelForNode(button, bTitle);
        button.setAttribute("class", "button-dark");
        button.setAttribute("oncommand",
          "document.getElementById('prompt-confirm-dialog').PromptHelper.closeConfirm(" + i + ")");
        bbox.appendChild(button);
      }
      
      aButtonFlags >>= 8;
    }
    
    dialog.waitForClose();
    return params.result;
  },
  
  commonPrompt : function(aParent, aTitle, aText, aValue, aCheckMsg, aCheckState, isPassword) {
    let doc = this.getDocument();
    if (!doc) {
      throw "No document !";
    }
    
    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;
    params.value = aValue;
    
    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/prompt.xul", params);
    doc.getElementById("prompt-prompt-title").value = aTitle;
    doc.getElementById("prompt-prompt-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-prompt-message", 80);
    this.sizeScrollableMsg("prompt-prompt-message", 25);

    doc.getElementById("prompt-prompt-checkbox").checked = aCheckState.value;
    this.setLabelForNode(doc.getElementById("prompt-prompt-checkbox-msg"), aCheckMsg);
    this.sizeElement("prompt-prompt-checkbox-msg", 50);
    doc.getElementById("prompt-prompt-textbox").value = aValue.value;
    if (aCheckMsg) {
      doc.getElementById("prompt-prompt-checkbox-box").removeAttribute("collapsed");
    }
    if (isPassword) {
      doc.getElementById("prompt-prompt-textbox").type = "password";
    }
    
    dialog.waitForClose();
    return params.result;
  },
  
  prompt : function(aParent, aTitle, aText, aValue, aCheckMsg, aCheckState) {
    try {
      return this.commonPrompt(aParent, aTitle, aText, aValue, aCheckMsg, aCheckState, false);
    } catch(e) {
      return this._getFallbackService().prompt(aParent, aTitle, aText, aValue, aCheckMsg, aCheckState);
    }
  },
  
  promptPassword: function(aParent, aTitle, aText, aPassword, aCheckMsg, aCheckState) {
    try {
      return this.commonPrompt(aParent, aTitle, aText, aPassword, aCheckMsg, aCheckState, true);
    } catch(e) {
      return this._getFallbackService().promptPassword(aParent, aTitle, aText, aPassword, aCheckMsg, aCheckState);
    }
  },
  
  promptUsernameAndPassword: function(aParent, aTitle, aText, aUsername, aPassword, aCheckMsg, aCheckState) {
    let doc = this.getDocument();
    if (!doc) {
      return this._getFallbackService().promptUsernameAndPassword(aParent, aTitle, aText, aUsername, aPassword, aCheckMsg, aCheckState);
    }

    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;
    params.user = aUsername;
    params.password = aPassword;

    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/promptPassword.xul", params);
    doc.getElementById("prompt-password-title").value = aTitle;
    doc.getElementById("prompt-password-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-password-message", 80);
    this.sizeScrollableMsg("prompt-password-message", 25);
    doc.getElementById("prompt-password-checkbox").checked = aCheckState.value;
    
    doc.getElementById("prompt-password-user").value = aUsername.value;
    doc.getElementById("prompt-password-password").value = aPassword.value;
    if (aCheckMsg) {
      doc.getElementById("prompt-password-checkbox-box").removeAttribute("collapsed");
      this.setLabelForNode(doc.getElementById("prompt-password-checkbox-msg"), aCheckMsg);
      this.sizeElement("prompt-password-checkbox-msg", 50);
      this.sizeElement("prompt-password-checkbox-box", 50);
    }
    
    dialog.waitForClose();
    return params.result;
  },
  
  //
  // JS port of http://mxr.mozilla.org/mozilla-central/source/embedding/components/windowwatcher/public/nsPromptUtils.h#89
  //
  getAuthHostPort: function(aChannel, aAuthInfo) {
    let uri = aChannel.URI;
    let res = { host: null, port: -1 };
    if (aAuthInfo.flags & aAuthInfo.AUTH_PROXY) {
      let proxy = aChannel.QueryInterface(Ci.nsIProxiedChannel);
      res.host = proxy.proxyInfo.host;
      res.port = proxy.proxyInfo.port;
    } else {
      res.host = uri.host;
      res.port = uri.port;
    }
    return res;
  },
  
  //
  // JS port of http://mxr.mozilla.org/mozilla-central/source/embedding/components/windowwatcher/src/nsPrompt.cpp#388
  //
  makeDialogText: function(aChannel, aAuthInfo) {
    let bundleService = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
    let bundle = bundleService.createBundle("chrome://global/locale/prompts.properties");
    
    let HostPort = this.getAuthHostPort(aChannel, aAuthInfo);
    let displayHost = HostPort.host;
    let uri = aChannel.URI;
    let scheme = uri.scheme;
    let username = aAuthInfo.username;
    let proxyAuth = (aAuthInfo.flags & aAuthInfo.AUTH_PROXY) != 0;
    let realm = aAuthInfo.realm;
    if (realm.length > 100) { // truncate and add ellipsis
      let pref = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      let ellipsis = pref.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
      if (!ellipsis)
        ellipsis = "...";
      realm = realm.substring(0, 100) + ellipsis;
    }
    
    if (HostPort.port != -1)
      displayHost += ":" + HostPort.port;
    
    let text = null;
    if (proxyAuth) {
      text = "EnterLoginForProxy";
    } else {
      text = "EnterLoginForRealm";
      displayHost = scheme + "://" + displayHost;
    }
    
    let strings = [realm, displayHost];
    let count = 2;
    if (aAuthInfo.flags & aAuthInfo.ONLY_PASSWORD) {
      text = "EnterPasswordFor";
      strings[0] = username;
    } else if (!proxyAuth && (realm.length == 0)) {
      text = "EnterUserPasswordFor";
      count = 1;
      strings[0] = strings[1];
    }
    
    return bundle.formatStringFromName(text, strings, count);
  },
  
  promptAuth: function(aParent, aChannel, aLevel, aAuthInfo, aCheckMsg, aCheckState) {
    let res = false;
    
    let defaultUser = aAuthInfo.username;
    if ((aAuthInfo.flags & aAuthInfo.NEED_DOMAIN) && (aAuthInfo.domain.length > 0))
      defaultUser = aAuthInfo.domain + "\\" + defaultUser;
    
    let username = { value: defaultUser };
    let password = { value: aAuthInfo.password };
    
    let message = this.makeDialogText(aChannel, aAuthInfo);
    let title = this.getLocaleString("PromptUsernameAndPassword2");
    
    if (aAuthInfo.flags & aAuthInfo.ONLY_PASSWORD) {
      res = this.promptPassword(aParent, title, message, password, aCheckMsg, aCheckState);
    } else {
      res = this.promptUsernameAndPassword(aParent, title, message, username, password, aCheckMsg, aCheckState);
    }
    
    if (res) {
      aAuthInfo.username = username.value;
      aAuthInfo.password = password.value;
    }
    
    return res;
  },
  
  asyncPromptAuth: function(aParent, aChannel, aCallback, aContext, aLevel, aAuthInfo, aCheckMsg, aCheckState) {
    // bug 514196
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  select: function(aParent, aTitle, aText, aCount, aSelectList, aOutSelection) {
    let doc = this.getDocument();
    if (!doc) {
      return this._getFallbackService().select(aParent, aTitle, aText, aCount, aSelectList, aOutSelection);
    }

    var params = new Object();
    params.result = false;
    params.selection = aOutSelection;

    let dialog = this.openDialog(aParent, "chrome://browser/content/prompt/select.xul", params);
    doc.getElementById("prompt-select-title").value = aTitle;
    doc.getElementById("prompt-select-message").appendChild(doc.createTextNode(aText));
    this.sizeElement("prompt-select-message", 80);
    this.sizeScrollableMsg("prompt-select-message", 25);
    
    let list = doc.getElementById("prompt-select-list");
    for (let i = 0; i < aCount; i++)
      list.appendItem(aSelectList[i], null, null);
      
    // select the first one
    list.selectedIndex = 0;
    
    dialog.waitForClose();
    return params.result;
  }
};

//module initialization
function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([promptService]);
}
