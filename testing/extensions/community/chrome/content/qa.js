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
 * The Original Code is the Mozilla Community QA Extension
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Zach Lipton <zach@zachlipton.com>
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


var qaMain = {
  htmlNS: "http://www.w3.org/1999/xhtml",

  openQATool : function() {
    window.open("chrome://qa/content/qa.xul", "_blank",
                "chrome,all,dialog=no,resizable=yes");
  },
  onToolOpen : function() {
    if (qaPref.getPref(qaPref.prefBase+'.isFirstTime', 'bool') == true) {
      window.open("chrome://qa/content/setup.xul", "_blank",
                  "chrome,all,dialog=yes");
        }
    else {
      // We need to log the user into litmus
      var storedLogin = qaPref.litmus.getPasswordObj();
      this.correctCredentials(storedLogin.username, storedLogin.password, false);
    }
    if (qaPref.getPref(qaPref.prefBase + '.currentTestcase.testrunSummary', 'char') != null) {
            litmus.readStateFromPref();
        }
  },
    onSwitchTab : function() {
    var newSelection = $('qa_tabrow').selectedItem;

    // user is switching to the prefs tab:
    if ($('qa_tabrow').selectedItem == $('qa-tabbar-prefs')) {
            qaPrefsWindow.loadPrefsWindow();
    } else if ($('qa_tabrow').selectedItem == $('qa-tabbar-bugzilla')) {
            bugzilla.unhighlightTab();
        }

    // user is switching away from the prefs tab:
    if (qaPrefsWindow.lastSelectedTab != null &&
        qaPrefsWindow.lastSelectedTab == $('qa-tabbar-prefs')) {
      qaPrefsWindow.savePrefsWindow();
    }

    qaPrefsWindow.lastSelectedTab = newSelection;
  },

  correctCredentials : function(username, password,isSecondTry) {
    var callback = function (resp) {
      if (resp.responseText == 0) {
        qaMain.doLogin(isSecondTry);
      } else {
        // Then we need to store our validated creds
        qaPref.litmus.setPassword(username, password);
      }
    }

    // First we validate our stored login.
    litmus.validateLogin(username, password, callback);
  },

  doLogin : function(isSecondTry) {
    try {
      var username = {value: "username"};
      var password = {value: "password"};
      var check = {value: "null"};
      var title = qaMain.bundle.getString("qa.getpassword.title");
      var msg = "";

      if (!isSecondTry)
        msg = qaMain.bundle.getString("qa.getpassword.message");
      else
        msg = qaMain.bundle.getString("qa.getpassword.tryagainmessage");

      var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                              .getService(Components.interfaces.nsIPromptService);
      var result = prompts.promptUsernameAndPassword(null, title, msg, username,
                                                     password, null, check);

      this.correctCredentials(username.value, password.value, true);
    } catch(ex) {
      alert("ERROR LOGGING IN: " + ex);
      dump("Error logging in: " + ex);
    }
  }
};

qaMain.__defineGetter__("bundle", function(){return $("bundle_qa");});
qaMain.__defineGetter__("urlbundle", function(){return $("bundle_urls");});
function $() {
  var elements = new Array();

for (var i = 0; i < arguments.length; i++) {
  var element = arguments[i];
  if (typeof element == 'string')
    element = document.getElementById(element);

  if (arguments.length == 1)
    return element;

  elements.push(element);
  }

  return elements;
}
