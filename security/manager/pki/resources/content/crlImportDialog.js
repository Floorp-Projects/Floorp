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
 *  Rangan Sen <rangansen@netscape.com>
 */

const nsPKIParamBlock    = "@mozilla.org/security/pkiparamblock;1";
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;
const nsICRLInfo          = Components.interfaces.nsICRLInfo;
const nsIPrefService      = Components.interfaces.nsIPrefService

var pkiParams;
var cert;
var crl;

function onLoad()
{
  pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);  
  isupport = pkiParams.getISupportAtIndex(1);
  if (isupport) {
    crl = isupport.QueryInterface(nsICRLInfo);
  }
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var yesButton = bundle.GetStringFromName("yesButton");
  var noButton = bundle.GetStringFromName("noButton");
  document.documentElement.getButton("accept").label = yesButton;
  document.documentElement.getButton("cancel").label = noButton;
  
  var nextUpdateStr;
  var orgStr;
  var orgUnitStr;

  if(crl != null) {    
    nextUpdateStr = crl.nextUpdateLocale;
    if( (nextUpdateStr == null) || (nextUpdateStr.length == 0) ){
      nextUpdateStr = bundle.GetStringFromName("undefinedValStr");
    }
    var nextUpdate = document.getElementById("nextUpdate");
    nextUpdate.setAttribute("value",nextUpdateStr);
    var org = document.getElementById("orgText");
    org.setAttribute("value", crl.organization);
    var orgUnit = document.getElementById("orgUnitText");
    orgUnit.setAttribute("value", crl.organizationalUnit);

    var autoupdateEnabledString   = "security.crl.autoupdate.enable." + crl.nameInDb;
    
    var updateEnabled = false;
    try {
      var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefService);
      var prefBranch = prefService.getBranch(null);
      updateEnabled = prefBranch.getBoolPref(autoupdateEnabledString);
      if(updateEnabled) {
        var autoupdateURLString       = "security.crl.autoupdate.url." + crl.nameInDb;
        prefBranch.setCharPref(autoupdateURLString, crl.lastFetchURL);
        prefService.savePrefFile(null);
      }
    }catch(exception){}

    var statement = document.getElementById("status");
    var question = document.getElementById("question");
    if(updateEnabled) {
      statement.setAttribute("value",bundle.GetStringFromName("enabledStatement"));
      question.setAttribute("value",bundle.GetStringFromName("crlAutoupdateQuestion2"));
    } else {
      statement.setAttribute("value",bundle.GetStringFromName("disabledStatement"));
      question.setAttribute("value",bundle.GetStringFromName("crlAutoupdateQuestion1"));
    }
  }  
}

function onCancel()
{
  return true;
}


function onAccept()
{
  var params = Components.classes[nsPKIParamBlock].createInstance(nsIPKIParamBlock);
  params.setISupportAtIndex(1, crl);
  
  window.openDialog("chrome://pippki/content/pref-crlupdate.xul","",
                    "chrome,centerscreen,modal",params);
  return true;
}

function doHelpButton()
{
  openHelp("validation-crl-import");
}

