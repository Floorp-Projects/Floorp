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

const nsICRLManager = Components.interfaces.nsICRLManager;
const nsCRLManager  = "@mozilla.org/security/crlmanager;1";
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsICRLInfo          = Components.interfaces.nsICRLInfo;
const nsIPref             = Components.interfaces.nsIPref;
 
var crl;
var bundle;
var prefs;
var updateTypeRadio;
var enabledCheckBox;
var timeBasedRadio;
var freqBasedRadio;
var crlManager;

var autoupdateEnabledString   = "security.crl.autoupdate.enable.";
var autoupdateTimeTypeString  = "security.crl.autoupdate.timingType.";
var autoupdateTimeString      = "security.crl.autoupdate.nextInstant.";
var autoupdateURLString       = "security.crl.autoupdate.url.";
var autoupdateErrCntString    = "security.crl.autoupdate.errCount.";
var autoupdateErrDetailString = "security.crl.autoupdate.errDetail.";
var autoupdateDayCntString    = "security.crl.autoupdate.dayCnt.";
var autoupdateFreqCntString   = "security.crl.autoupdate.freqCnt.";

function onLoad()
{
  crlManager = Components.classes[nsCRLManager].getService(nsICRLManager);
  var pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);  
  var isupport = pkiParams.getISupportAtIndex(1);
  crl = isupport.QueryInterface(nsICRLInfo);

  autoupdateEnabledString    = autoupdateEnabledString + crl.nameInDb;
  autoupdateTimeTypeString  = autoupdateTimeTypeString + crl.nameInDb;
  autoupdateTimeString      = autoupdateTimeString + crl.nameInDb;
  autoupdateDayCntString    = autoupdateDayCntString + crl.nameInDb;
  autoupdateFreqCntString   = autoupdateFreqCntString + crl.nameInDb;
  autoupdateURLString       = autoupdateURLString + crl.nameInDb;
  autoupdateErrCntString    = autoupdateErrCntString + crl.nameInDb;
  autoupdateErrDetailString = autoupdateErrDetailString + crl.nameInDb;

  bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  prefs = Components.classes["@mozilla.org/preferences;1"].getService(nsIPref);

  updateTypeRadio = document.getElementById("autoUpdateType");
  enabledCheckBox = document.getElementById("enableCheckBox");
  timeBasedRadio = document.getElementById("timeBasedRadio");
  freqBasedRadio = document.getElementById("freqBasedRadio");

  //Read the existing prefs, if any
  initializeSelection();
}

function updateSelectedTimingControls()
{
  var freqBox = document.getElementById("nextUpdateFreq");
  var timeBox = document.getElementById("nextUpdateDay");
  if(updateTypeRadio.selectedItem.id == "freqBasedRadio"){
    freqBox.removeAttribute("disabled");
    timeBox.disabled = true;
  } else {
    timeBox.removeAttribute("disabled");
    freqBox.disabled = true;
  }
}

function initializeSelection()
{
  var menuItemNode;
  var hasAdvertisedURL = false;
  var hasNextUpdate = true;

  var lastFetchMenuNode;
  var advertisedMenuNode;
  
  try {
    var isEnabled = prefs.GetBoolPref(autoupdateEnabledString);
    enabledCheckBox.checked = isEnabled;
  } catch(exception){
    enabledCheckBox.checked = false;
  }

  //Always the last fetch url, for now.
  var URLDisplayed = document.getElementById("urlName"); 
  URLDisplayed.value = crl.lastFetchURL;
  
  //Decide how many update timing types to be shown
  //If no next update specified, hide the first choice. Default shows both
  if(crl.nextUpdateLocale == null || crl.nextUpdateLocale.length == 0) {
    timeBasedRadio.disabled = true;
    hasNextUpdate = false;
  }
  
  //Set up the initial selections based on defaults and prefs, if any
  try{
    var timingPref = prefs.GetIntPref(autoupdateTimeTypeString);
    if(timingPref != null) {
      if(timingPref == crlManager.TYPE_AUTOUPDATE_TIME_BASED) {
        if(hasNextUpdate){
          updateTypeRadio.selectedItem = timeBasedRadio;
        }
      } else {
        updateTypeRadio.selectedItem = freqBasedRadio;
      }
    } else {
      if(hasNextUpdate){
        updateTypeRadio.selectedItem = timeBasedRadio;
      } else {
        updateTypeRadio.selectedItem = freqBasedRadio;
      }
    }
    
  }catch(exception){
    if(!hasNextUpdate) {
      updateTypeRadio.selectedItem = freqBasedRadio;
    } else {
      updateTypeRadio.selectedItem = timeBasedRadio;
    }
  }

  updateSelectedTimingControls();

  //Now, retrieving the day count
  var timeBasedBox = document.getElementById("nextUpdateDay");
  try {
    var dayCnt = prefs.GetCharPref(autoupdateDayCntString);
    //alert(dayCnt);
    if(dayCnt != null){
      timeBasedBox.value = dayCnt;
    } else {
      timeBasedBox.value = 1; 
    }
  } catch(exception) {
    timeBasedBox.value = 1;
  }

  var freqBasedBox = document.getElementById("nextUpdateFreq");
  try {
    var freqCnt = prefs.GetCharPref(autoupdateFreqCntString);
    //alert(freqCnt);
    if(freqCnt != null){
      freqBasedBox.value = freqCnt;
    } else {
      freqBasedBox.value = 1; 
    }
  } catch(exception) {
    freqBasedBox.value = 1;
  }

  var errorCountText = document.getElementById("FailureCnt");
  var errorDetailsText = document.getElementById("FailureDetails");
  var cnt = 0;
  var text;
  try{
    cnt = prefs.GetIntPref(autoupdateErrCntString);
    txt = prefs.GetCharPref(autoupdateErrDetailString);
  }catch(exception){}

  if( cnt > 0 ){
    errorCountText.setAttribute("value",cnt);
    errorDetailsText.setAttribute("value",txt);
  } else {
    errorCountText.setAttribute("value",bundle.GetStringFromName("NoUpdateFailure"));
    var reasonBox = document.getElementById("reasonbox");
    reasonBox.hidden = true;
  }
}

function onCancel()
{
  // Close dialog by returning true
  return true;
}

function doHelpButton()
{
  openHelp("validation-crl-auto-update-prefs");
}

function onAccept()
{
   if(!validatePrefs())
     return false;

   //set enable pref
   prefs.SetBoolPref(autoupdateEnabledString, enabledCheckBox.checked );
   
   //set URL TYPE and value prefs - always to last fetch url - till we have anything else available
   prefs.SetCharPref(autoupdateURLString,crl.lastFetchURL);
   
   var timingTypeId = updateTypeRadio.selectedItem.id;
   var updateTime;
   var dayCnt = (document.getElementById("nextUpdateDay")).value;
   var freqCnt = (document.getElementById("nextUpdateFreq")).value;

   if(timingTypeId == "timeBasedRadio"){
     prefs.SetIntPref(autoupdateTimeTypeString,crlManager.TYPE_AUTOUPDATE_TIME_BASED);
     updateTime = crlManager.computeNextAutoUpdateTime(crl, crlManager.TYPE_AUTOUPDATE_TIME_BASED, dayCnt);
   } else {
     prefs.SetIntPref(autoupdateTimeTypeString,crlManager.TYPE_AUTOUPDATE_FREQ_BASED);
     updateTime = crlManager.computeNextAutoUpdateTime(crl, crlManager.TYPE_AUTOUPDATE_FREQ_BASED, freqCnt);
   }

   //alert(updateTime);
   prefs.SetCharPref(autoupdateTimeString,updateTime); 
   prefs.SetCharPref(autoupdateDayCntString,dayCnt);
   prefs.SetCharPref(autoupdateFreqCntString,freqCnt);

   //Save Now
   prefs.savePrefFile(null);
   
   crlManager.rescheduleCRLAutoUpdate();
   //Close dialog by returning true
   return true;
}

function validatePrefs()
{
   var dayCnt = (document.getElementById("nextUpdateDay")).value;
   var freqCnt = (document.getElementById("nextUpdateFreq")).value;

   var tmp = parseFloat(dayCnt);
   if(!(tmp > 0.0)){
     alert(bundle.GetStringFromName("crlAutoUpdateDayCntError"));
     return false;
   }
   
   tmp = parseFloat(freqCnt);
   if(!(tmp > 0.0)){
     alert(bundle.GetStringFromName("crlAutoUpdtaeFreqCntError"));
     return false;
   }
   
   return true;
}
