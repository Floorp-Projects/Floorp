/*
 * $_
 *
 * Returns the element with the specified |name| attribute.
 */
function $_(formNum, name) {
  var form = document.getElementById("form" + formNum);
  if (!form) {
    logWarning("$_ couldn't find requested form " + formNum);
    return null;
  }

  var element = form.elements.namedItem(name);
  if (!element) {
    logWarning("$_ couldn't find requested element " + name);
    return null;
  }

  // Note that namedItem is a bit stupid, and will prefer an
  // |id| attribute over a |name| attribute when looking for
  // the element. Login Mananger happens to use .namedItem
  // anyway, but let's rigorously check it here anyway so
  // that we don't end up with tests that mistakenly pass.

  if (element.getAttribute("name") != name) {
    logWarning("$_ got confused.");
    return null;
  }

  return element;
}


/*
 * checkForm
 *
 * Check a form for expected values. If an argument is null, a field's
 * expected value will be the default value.
 *
 * <form id="form#">
 * checkForm(#, "foo");
 */
function checkForm(formNum, val1, val2, val3) {
    var e, form = document.getElementById("form" + formNum);
    ok(form, "Locating form " + formNum);

    var numToCheck = arguments.length - 1;
    
    if (!numToCheck--)
        return;
    e = form.elements[0];
    if (val1 == null)
        is(e.value, e.defaultValue, "Test default value of field " + e.name +
            " in form " + formNum);
    else
        is(e.value, val1, "Test value of field " + e.name +
            " in form " + formNum);


    if (!numToCheck--)
        return;
    e = form.elements[1];
    if (val2 == null)
        is(e.value, e.defaultValue, "Test default value of field " + e.name +
            " in form " + formNum);
    else
        is(e.value, val2, "Test value of field " + e.name +
            " in form " + formNum);


    if (!numToCheck--)
        return;
    e = form.elements[2];
    if (val3 == null)
        is(e.value, e.defaultValue, "Test default value of field " + e.name +
            " in form " + formNum);
    else
        is(e.value, val3, "Test value of field " + e.name +
            " in form " + formNum);

}


/*
 * checkUnmodifiedForm
 *
 * Check a form for unmodified values from when page was loaded.
 *
 * <form id="form#">
 * checkUnmodifiedForm(#);
 */
function checkUnmodifiedForm(formNum) {
    var form = document.getElementById("form" + formNum);
    ok(form, "Locating form " + formNum);

    for (var i = 0; i < form.elements.length; i++) {
        var ele = form.elements[i];

        // No point in checking form submit/reset buttons.
        if (ele.type == "submit" || ele.type == "reset")
            continue;

        is(ele.value, ele.defaultValue, "Test to default value of field " +
            ele.name + " in form " + formNum);
    }
}


// Mochitest gives us a sendKey(), but it's targeted to a specific element.
// This basically sends an untargeted key event, to whatever's focused.
function doKey(aKey, modifier) {
    // Seems we need to enable this again, or sendKeyEvent() complaints.
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

    var keyName = "DOM_VK_" + aKey.toUpperCase();
    var key = Components.interfaces.nsIDOMKeyEvent[keyName];

    // undefined --> null
    if (!modifier)
        modifier = null;

    // Window utils for sending fake sey events.
    var wutils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                          getInterface(Components.interfaces.nsIDOMWindowUtils);

    if (wutils.sendKeyEvent("keydown",  key, 0, modifier)) {
      wutils.sendKeyEvent("keypress", key, 0, modifier);
    }
    wutils.sendKeyEvent("keyup",    key, 0, modifier);
}

// Init with a common login
function commonInit() {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

    var pwmgr = Components.classes["@mozilla.org/login-manager;1"].
                getService(Components.interfaces.nsILoginManager);
    ok(pwmgr != null, "Access LoginManager");


    // Check that initial state has no logins
    var logins = pwmgr.getAllLogins();
    if (logins.length) {
        //todo(false, "Warning: wasn't expecting logins to be present.");
        pwmgr.removeAllLogins();
    }
    var disabledHosts = pwmgr.getAllDisabledHosts();
    if (disabledHosts.length) {
        //todo(false, "Warning: wasn't expecting disabled hosts to be present.");
        for (var host of disabledHosts)
            pwmgr.setLoginSavingEnabled(host, true);
    }

    // Add a login that's used in multiple tests
    var login = Components.classes["@mozilla.org/login-manager/loginInfo;1"].
                createInstance(Components.interfaces.nsILoginInfo);
    login.init("http://mochi.test:8888", "http://mochi.test:8888", null,
               "testuser", "testpass", "uname", "pword");
    pwmgr.addLogin(login);

    // Last sanity check
    logins = pwmgr.getAllLogins();
    is(logins.length, 1, "Checking for successful init login");
    disabledHosts = pwmgr.getAllDisabledHosts();
    is(disabledHosts.length, 0, "Checking for no disabled hosts");
}

const masterPassword = "omgsecret!";

function enableMasterPassword() {
    setMasterPassword(true);
}

function disableMasterPassword() {
    setMasterPassword(false);
}

function setMasterPassword(enable) {
    var oldPW, newPW;
    if (enable) {
        oldPW = "";
        newPW = masterPassword;
    } else {
        oldPW = masterPassword;
        newPW = "";
    }
    // Set master password. Note that this does not log you in, so the next
    // invocation of pwmgr can trigger a MP prompt.

    var pk11db = Cc["@mozilla.org/security/pk11tokendb;1"].
                 getService(Ci.nsIPK11TokenDB)
    var token = pk11db.findTokenByName("");
    ok(true, "change from " + oldPW + " to " + newPW);
    token.changePassword(oldPW, newPW);
}

function logoutMasterPassword() {
    var sdr = Cc["@mozilla.org/security/sdr;1"].
            getService(Ci.nsISecretDecoderRing);
    sdr.logoutAndTeardown();
}

function dumpLogins(pwmgr) {
    var logins = pwmgr.getAllLogins();
    ok(true, "----- dumpLogins: have " + logins.length + " logins. -----");
    for (var i = 0; i < logins.length; i++)
        dumpLogin("login #" + i + " --- ", logins[i]);
}

function dumpLogin(label, login) {
    loginText = "";
    loginText += "host: ";
    loginText += login.hostname;
    loginText += " / formURL: ";
    loginText += login.formSubmitURL;
    loginText += " / realm: ";
    loginText += login.httpRealm;
    loginText += " / user: ";
    loginText += login.username;
    loginText += " / pass: ";
    loginText += login.password;
    loginText += " / ufield: ";
    loginText += login.usernameField;
    loginText += " / pfield: ";
    loginText += login.passwordField;
    ok(true, label + loginText);
}
