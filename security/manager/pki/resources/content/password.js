/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bob Lord <lord@netscape.com>
 *  Terry Hayes <thayes@netscape.com>
 */
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Components.interfaces.nsIPKCS11ModuleDB;
const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;

var params;
var tokenName;
var pw1;

function onLoad()
{
  pw1 = document.getElementById("pw1");

  if ("arguments" in window) {
    params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    tokenName = params.GetString(1);
  } else {
    tokenName = self.name;
  }

  // Set token name in display
  var t = document.getElementById("tokenName");
  t.setAttribute("value", tokenName);

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

  // If the token is unitialized, don't use the old password box.
  // Otherwise, do.
  var secmoddb = Components.
                  classes[nsPKCS11ModuleDB].
                  getService(nsIPKCS11ModuleDB);
  var slot = secmoddb.findSlotByName(tokenName);
  if (slot) {
    var oldpwbox = document.getElementById("oldpw");
    if (slot.status == nsIPKCS11Slot.SLOT_UNINITIALIZED ) {
      oldpwbox.setAttribute("disabled", "true");
      oldpwbox.setAttribute("type", "text");
      oldpwbox.setAttribute("value", 
                            bundle.GetStringFromName("password_not_set")); 
      oldpwbox.setAttribute("inited", "true");
      // Select first password field
      document.getElementById('pw1').focus();
    } else {
      // Select old password field
      oldpwbox.setAttribute("inited", "false");
      oldpwbox.focus();
    }
  }
}

function onP12Load()
{
  pw1 = document.getElementById("pw1");
  params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  // Select first password field
  document.getElementById('pw1').focus();
}

function setPassword()
{
  var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var token = pk11db.findTokenByName(tokenName);

  var oldpwbox = document.getElementById("oldpw");
  var initpw = oldpwbox.getAttribute("inited");
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  if (initpw == "false") {
    try {
      var passok = token.checkPassword(oldpwbox.value);
      if (passok) {
        token.changePassword(oldpwbox.value, pw1.value);
        alert(bundle.GetStringFromName("pw_change_ok")); 
      } else {
        alert(bundle.GetStringFromName("incorrect_pw")); 
      }
    } catch (e) {
      alert(bundle.GetStringFromName("failed_pw_change")); 
    }
  } else {
    token.initPassword(pw1.value);
  }

  if (params) {
    // Return value
    params.SetInt(1, 1);
  }

  // Terminate dialog
  window.close();
}

function getPassword()
{
  // grab what was entered
  params.SetString(2, pw1.value);
  // Return value
  params.SetInt(1, 1);
  // Terminate dialog
  window.close();
}

function setP12Password()
{
  // grab what was entered
  params.SetString(2, pw1.value);
  // Return value
  params.SetInt(1, 1);
  // Terminate dialog
  window.close();
}

function setPasswordStrength()
{
// Here is how we weigh the quality of the password
// number of characters
// numbers
// non-alpha-numeric chars
// upper and lower case characters

  var pw=document.getElementById('pw1').value;
//  alert("password='" + pw +"'");

//length of the password
  var pwlength=(pw.length);
  if (pwlength>5)
    pwlength=5;


//use of numbers in the password
  var numnumeric = pw.replace (/[0-9]/g, "");
  var numeric=(pw.length - numnumeric.length);
  if (numeric>3)
    numeric=3;

//use of symbols in the password
  var symbols = pw.replace (/\W/g, "");
  var numsymbols=(pw.length - symbols.length);
  if (numsymbols>3)
    numsymbols=3;

//use of uppercase in the password
  var numupper = pw.replace (/[A-Z]/g, "");
  var upper=(pw.length - numupper.length);
  if (upper>3)
    upper=3;


  var pwstrength=((pwlength*10)-20) + (numeric*10) + (numsymbols*15) + (upper*10);

  var mymeter=document.getElementById('pwmeter');
  mymeter.setAttribute("value",pwstrength);

  return;
}

function checkPasswords()
{
  var pw1=document.getElementById('pw1').value;
  var pw2=document.getElementById('pw2').value;

  var ok=document.getElementById('ok-button');

  if (pw1 == "") {
    ok.setAttribute("disabled","true");
    return;
  }

  if (pw1 == pw2){
    ok.setAttribute("disabled","false");
  } else
  {
    ok.setAttribute("disabled","true");
  }

}
