/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var qaPrefsWindow = {
  lastSelectedTab : null,

  loadPrefsWindow : function() {
    prefsTabOpen = true;
    document.getElementById('qa-preferences-litmus-username').value =
      qaPref.litmus.getUsername() || '';
    document.getElementById('qa-preferences-litmus-password').value =
      qaPref.litmus.getPassword() || '';

    // load notification settings:
    var prefs = qaPref.getPref(qaPref.prefBase+'.notificationSettings', 'char');
    prefs = prefs.split(",");
    var notify = $('qa-prefs-notify').childNodes;
    var prefCounter = 0;
    for (var i=0; i<notify.length; i++) {
      if (notify[i].setChecked != null && notify[i].checked != null) { // it's a checkbox
        if (prefs[prefCounter] == "1")
          notify[i].checked = true;
        else
          notify[i].checked = false;
        prefCounter++;
      }
    }
  }, 
  savePrefsWindow : function() {
    // save notification settings
    var notify = $('qa-prefs-notify').childNodes;
    var prefs = '';
    for (var i=0; i<notify.length; i++) {
      if (notify[i].setChecked != null && (notify[i].checked == true
          || notify[i].checked == false)) { // it's a checkbox
        if (notify[i].checked == false)
          prefs += '0,';
        else
          prefs += '1,';
      }
    }
    prefs = prefs.substring(0, prefs.length-1); // remove the trailing comma
    qaPref.setPref(qaPref.prefBase+'.notificationSettings', prefs, 'char');

    // save litmus account settings
    var uname = document.getElementById('qa-preferences-litmus-username').value;
    var passwd = document.getElementById('qa-preferences-litmus-password').value;

    // if uname and passwd are unchanged, we're done:
    if (uname == qaPref.litmus.getUsername() &&
        passwd == qaPref.litmus.getPassword())
      return;
    var callback = function(resp) {
      if (resp.responseText == 0) {
        alert(document.getElementById("bundle_qa").
          getString("qa.extension.prefs.loginError"));
        // snap the tab selection back to prefs:
        $('qa_tabrow').selectedItem = $('qa-tabbar-prefs');
        return false;
      } else {
        qaPref.litmus.setPassword(uname, passwd);
        return true;
      }
    }
    litmus.validateLogin(uname, passwd, callback);
    return false; // not ready to close yet
  },
  loadUsernameAndPassword : function() {
    var uname = document.getElementById('qa-preferences-litmus-username');
    var passwd = document.getElementById('qa-preferences-litmus-password');

    uname.value = qaPref.litmus.getUsername();
    passwd.value = qaPref.litmus.getPassword();
  },
  createAccount : function() {
    window.openDialog("chrome://qa/content/accountcreate.xul",
                      "_blank",
                      "chrome,all,dialog=yes",
                      qaPrefsWindow.loadUsernameAndPassword);
  }
};

var qaSetup = {
  didSubmitForm : 0,

  hideAccountSettings : function() {
    var accountyes = document.getElementById('qa-setup-accountyes');
    var accountno = document.getElementById('qa-setup-accountno');
    accountyes.style.display = 'none';
    accountno.style.display = 'none';
  },
  loadAccountSettings : function() {
    var uname = document.getElementById('username');
    var passwd = document.getElementById('password');
    uname.value = qaPref.litmus.getUsername() || '';
    passwd.value = qaPref.litmus.getPassword() || '';
    if (qaPref.litmus.getUsername()) {
      document.getElementById("qa-setup-account-haveaccount").selectedIndex=1;
      document.getElementById('qa-setup-accountyes').style.display = '';
    }
    document.getElementById('qa-setup-createaccount-iframe').src =
      litmus.baseURL+'extension.cgi?createAccount=1';
  },

  accountSetting : function(yesno) {
    var accountyes = document.getElementById('qa-setup-accountyes');
    var accountno = document.getElementById('qa-setup-accountno');
    qaSetup.hideAccountSettings();
    if (yesno == '0') {
      accountno.style.display = '';
    } else {
      accountyes.style.display = '';
    }
  },
  retrieveAccount : function(frameid, loadingid) {
    var page = document.getElementById(frameid).contentDocument;
    if (!page) {
      alert("create account page is missing");
      return false;
    }
    if (page.wrappedJSObject == null)
      page.wrappedJSObject = page;
    if (page.forms[0] && page.forms[0].wrappedJSObject == null)
      page.forms[0].wrappedJSObject = page.forms[0];

    if (loadingid && page.location == litmus.baseURL+'extension.cgi?createAccount=1'
        && qaSetup.didSubmitForm==0) {
        document.getElementById('loadingid').value =
  document.getElementById("bundle_qa").getString("qa.extension.prefs.loadingMsg");
      page.forms[0].wrappedJSObject.submit();
      qaSetup.didSubmitForm = 1;
      setTimeout("qaSetup.validateAccount()", 5000);
      return false;
    }
    if (qaSetup.didSubmitForm == 1 && ! page.forms ||
       ! page.forms[0].wrappedJSObject ||
       ! page.forms[0].wrappedJSObject.email &&
       ! page.forms[0].wrappedJSObject.email.value)
        {qaSetup.didSubmitForm = 2;
        setTimeout("qaSetup.validateAccount()", 4000);
        return false;}
    var e = '';
    var p = '';
    if (page.forms && page.forms[0].wrappedJSObject &&
        page.forms[0].wrappedJSObject.email &&
      page.forms[0].wrappedJSObject.email.value)
      { e=page.forms[0].wrappedJSObject.email.value }
    if (page.forms && page.forms[0].wrappedJSObject &&
        page.forms[0].wrappedJSObject.password &&
      page.forms[0].wrappedJSObject.password.value)
      { p=page.forms[0].wrappedJSObject.password.value }

    return { name : e, password : p};
  },
  validateAccount : function() {
    if (document.getElementById('qa-setup-accountno').style.display == '') {
      var account = qaSetup.retrieveAccount("qa-setup-createaccount-iframe", "qa-setup-accountconfirmloading");
      document.getElementById('username').value = account.name;
      document.getElementById('password').value = account.password;
    }

    document.getElementById('qa-setup-accountconfirmloading').value =
  document.getElementById("bundle_qa").getString("qa.extension.prefs.loadingMsg");

    var uname = document.getElementById('username').value;
    var passwd = document.getElementById('password').value;

    var callback = function(resp) {
      if (resp.responseText != 1) { // failure
        alert(document.getElementById("bundle_qa").
          getString("qa.extension.prefs.loginError"));
          document.getElementById('qa-setup-accountconfirmloading').value = null;
        return false;
      } else { // all's well
        qaPref.litmus.setPassword(uname, passwd);
        document.getElementById('qa-setup-accountconfirmloading').value = null;
        document.getElementById('qa-setup').pageIndex++; // advance
        return true;
      }
    }
    litmus.validateLogin(uname, passwd, callback);
    return false; // not ready to advance yet
  },
    
  loadSysconfig : function() {
    $('qa-setup-sysconfig-loading').value =
      $("bundle_qa").getString("qa.extension.sysconfig.loadingMsg");
    var guessInfo = function() {
      var sysconfig;
      try {
        sysconfig = new Sysconfig();
      } catch (ex) {}
      var platItems = $('qa-setup-platform').menupopup.childNodes;
      for (var i=0; i<platItems.length; i++) {
        if (sysconfig.platform && platItems[i].label == sysconfig.platform)
          $('qa-setup-platform').selectedIndex = i;
      }
      var opsysItems = $('qa-setup-opsys').menupopup.childNodes;
      for (var i=0; i<opsysItems.length; i++) {
        if (sysconfig.opsys && opsysItems[i].label == sysconfig.opsys)
          $('qa-setup-opsys').selectedIndex = i;
      }
      $('qa-setup-sysconfig-loading').value = '';
    };

    qaTools.loadJsonMenu(litmus.baseURL+"/json.cgi?platforms=1",
        $('qa-setup-platform'), 'name', 'name');
      qaTools.loadJsonMenu(litmus.baseURL+"/json.cgi?opsyses=1",
        $('qa-setup-opsys'), 'name', 'name', guessInfo);
  },

  validateSysconfig : function() {
    var sysconfig;
    try {
      sysconfig = new Sysconfig();
    } catch (ex) {}

    // only set prefs for things which differ from the automatically
    // detected sysconfig for forward-compatibility
    if (! sysconfig.platform == $('qa-setup-platform').selectedItem.label) {
      qaPref.setPref(qaPref.prefBase+'.sysconfig.platform',
            $('qa-setup-platform').selectedItem.label, 'char');
    }
    if (! sysconfig.opsys == $('qa-setup-opsys').selectedItem.label) {
      qaPref.setPref(qaPref.prefBase+'.sysconfig.opsys',
            $('qa-setup-opsys').selectedItem.label, 'char');
    }
    return true;
  },

  finish : function() {
    qaPref.setPref(qaPref.prefBase+'.isFirstTime', false, 'bool');
  }
};
