/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
