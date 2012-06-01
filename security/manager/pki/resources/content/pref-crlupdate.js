/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsICRLManager = Components.interfaces.nsICRLManager;
const nsCRLManager  = "@mozilla.org/security/crlmanager;1";
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsICRLInfo          = Components.interfaces.nsICRLInfo;
const nsIPrefService      = Components.interfaces.nsIPrefService;
 
var crl;
var bundle;
var prefService;
var prefBranch;
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

function doPrompt(msg)
{
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  prompts.alert(window, null, msg);
}

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
  prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefService);
  prefBranch = prefService.getBranch(null);

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
    var isEnabled = prefBranch.getBoolPref(autoupdateEnabledString);
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
    var timingPref = prefBranch.getIntPref(autoupdateTimeTypeString);
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
    var dayCnt = prefBranch.getCharPref(autoupdateDayCntString);
    //doPrompt(dayCnt);
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
    var freqCnt = prefBranch.getCharPref(autoupdateFreqCntString);
    //doPrompt(freqCnt);
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
    cnt = prefBranch.getIntPref(autoupdateErrCntString);
    txt = prefBranch.getCharPref(autoupdateErrDetailString);
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

function onAccept()
{
   if(!validatePrefs())
     return false;

   //set enable pref
   prefBranch.setBoolPref(autoupdateEnabledString, enabledCheckBox.checked );
   
   //set URL TYPE and value prefs - always to last fetch url - till we have anything else available
   prefBranch.setCharPref(autoupdateURLString, crl.lastFetchURL);
   
   var timingTypeId = updateTypeRadio.selectedItem.id;
   var updateTime;
   var dayCnt = (document.getElementById("nextUpdateDay")).value;
   var freqCnt = (document.getElementById("nextUpdateFreq")).value;

   if(timingTypeId == "timeBasedRadio"){
     prefBranch.setIntPref(autoupdateTimeTypeString, crlManager.TYPE_AUTOUPDATE_TIME_BASED);
     updateTime = crlManager.computeNextAutoUpdateTime(crl, crlManager.TYPE_AUTOUPDATE_TIME_BASED, dayCnt);
   } else {
     prefBranch.setIntPref(autoupdateTimeTypeString, crlManager.TYPE_AUTOUPDATE_FREQ_BASED);
     updateTime = crlManager.computeNextAutoUpdateTime(crl, crlManager.TYPE_AUTOUPDATE_FREQ_BASED, freqCnt);
   }

   //doPrompt(updateTime);
   prefBranch.setCharPref(autoupdateTimeString, updateTime); 
   prefBranch.setCharPref(autoupdateDayCntString, dayCnt);
   prefBranch.setCharPref(autoupdateFreqCntString, freqCnt);

   //Save Now
   prefService.savePrefFile(null);
   
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
     doPrompt(bundle.GetStringFromName("crlAutoUpdateDayCntError"));
     return false;
   }
   
   tmp = parseFloat(freqCnt);
   if(!(tmp > 0.0)){
     doPrompt(bundle.GetStringFromName("crlAutoUpdtaeFreqCntError"));
     return false;
   }
   
   return true;
}
