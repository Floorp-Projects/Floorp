/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Doron Rosenberg.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

const pref = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);

// need it globally, but can only set it in startup()
var data;

function doAllowWindowOpen(){
  data.scriptData["allowWindowOpenChanged"].value = !data.scriptData["allowWindowOpenChanged"].value;
}

function doWindowMoveResize(){
  data.scriptData["allowWindowMoveResizeChanged"].value = !data.scriptData["allowWindowMoveResizeChanged"].value;
}

function doWindowStatusChange(){
  data.scriptData["allowWindowStatusChangeChanged"].value = !data.scriptData["allowWindowStatusChangeChanged"].value;
}

function doWindowFlipChange(){
  data.scriptData["allowWindowFlipChanged"].value = !data.scriptData["allowWindowFlipChanged"].value;
}

function doAllowCookieSet(){
  data.scriptData["allowDocumentCookieSetChanged"].value = !data.scriptData["allowDocumentCookieSetChanged"].value;
}

function doAllowCookieGet(){
  data.scriptData["allowDocumentCookieGetChanged"].value = !data.scriptData["allowDocumentCookieGetChanged"].value;
}

function doAllowImageSrcChange(){
  data.scriptData["allowImageSrcChangeChanged"].value = !data.scriptData["allowImageSrcChangeChanged"].value;
}

function changeDisabledState(state){
  //Set the states of the groupbox children state based on the "javascript enabled" checkbox value
  document.getElementById("allowScriptsDescription").disabled = state;
  document.getElementById("allowWindowMoveResize").disabled = state;
  document.getElementById("allowWindowOpen").disabled = state;
  document.getElementById("allowImageSrcChange").disabled = state;
  document.getElementById("allowDocumentCookieSet").disabled = state;
  document.getElementById("allowDocumentCookieGet").disabled = state;
  document.getElementById("allowWindowStatusChange").disabled = state;
  document.getElementById("allowWindowFlip").disabled = state;
}

function javascriptEnabledChange(state){
  changeDisabledState(!state);
}

function getPrefValueForCheckbox(prefName){

  var prefValue;

  try {
    prefValue = pref.GetCharPref(prefName);
    
    if(prefValue != "allAccess" && prefValue != "sameOrigin"){
      return false; 
    }
  }
  catch(e) {}

  return true;
}

function Startup(){

  data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-scripts.xul"];

  //If scriptData does not exist, then it is the first time the panel was shown and we default to false 
  if (!("scriptData" in data)){
    var changedList = ["allowWindowOpenChanged", "allowWindowMoveResizeChanged",
                       "allowWindowStatusChangeChanged", "allowWindowFlipChanged",
                       "allowDocumentCookieSetChanged", "allowDocumentCookieGetChanged", 
                       "allowImageSrcChangeChanged"];
    data.scriptData = [];
    for(var run = 0; run < changedList.length; run++ ){
      data.scriptData[ changedList[run] ] = [];
      data.scriptData[ changedList[run] ].value = false;
    }

    try{
      document.getElementById("allowWindowOpen").checked = 
        !pref.GetBoolPref("dom.disable_open_during_load"); 
    } catch (e){
      //We will only get an error if the preference doesn't exist, when that happens we default to true
      document.getElementById("allowWindowOpen").checked = true;
    }

    //If one of the security capability prefs is set, then the checkbox becomes unchecked
    document.getElementById("allowWindowMoveResize").checked = 
      getPrefValueForCheckbox("capability.policy.default.Window.resizeTo") &&
      getPrefValueForCheckbox("capability.policy.default.Window.innerWidth.set") && 
      getPrefValueForCheckbox("capability.policy.default.Window.innerHeight.set") &&
      getPrefValueForCheckbox("capability.policy.default.Window.outerWidth.set") && 
      getPrefValueForCheckbox("capability.policy.default.Window.outerHeight.set") &&
      getPrefValueForCheckbox("capability.policy.default.Window.sizeToContent") && 
      getPrefValueForCheckbox("capability.policy.default.Window.resizeBy") &&
      getPrefValueForCheckbox("capability.policy.default.Window.screenX.set") && 
      getPrefValueForCheckbox("capability.policy.default.Window.screenY.set") &&
      getPrefValueForCheckbox("capability.policy.default.Window.moveTo") && 
      getPrefValueForCheckbox("capability.policy.default.Window.moveBy");

    document.getElementById("allowWindowFlip").checked = 
      getPrefValueForCheckbox("capability.policy.default.Window.focus");

    document.getElementById("allowWindowStatusChange").checked = 
      getPrefValueForCheckbox("capability.policy.default.Window.status");

    document.getElementById("allowImageSrcChange").checked = 
      getPrefValueForCheckbox("capability.policy.default.HTMLImageElement.src");

    document.getElementById("allowDocumentCookieGet").checked = 
      getPrefValueForCheckbox("capability.policy.default.HTMLDocument.cookie.get");

    document.getElementById("allowDocumentCookieSet").checked = 
      getPrefValueForCheckbox("capability.policy.default.HTMLDocument.cookie.set");

  } else { //not first time it was loaded, get default values from data 
 
    document.getElementById("allowWindowOpen").checked = data["allowWindowOpen"].checked; 

    document.getElementById("allowWindowMoveResize").checked = data["allowWindowMoveResize"].checked; 

    document.getElementById("allowWindowFlip").checked = data["allowWindowFlip"].checked; 
    document.getElementById("allowWindowStatusChange").checked = data["allowWindowStatusChange"].checked; 

    document.getElementById("allowImageSrcChange").checked = data["allowImageSrcChange"].checked; 

    document.getElementById("allowDocumentCookieSet").checked = data["allowDocumentCookieSet"].checked; 

    document.getElementById("allowDocumentCookieGet").checked = data["allowDocumentCookieGet"].checked; 

    document.getElementById("javascriptEnabled").checked = data["javascriptEnabled"].checked;

  }

  //If javascript is disabled, then disable the checkboxes
  if (!document.getElementById("javascriptEnabled").checked) changeDisabledState(true);

  parent.hPrefWindow.registerOKCallbackFunc(doOnOk);
}

function doOnOk(){

  //If a user makes a change to this panel, goes to another panel, and returns to this panel to 
  //make another change, then we cannot use data[elementName].  This is because data[elementName] 
  //contains the original xul change and we would loose the new change. Thus we track all changes
  //by using getElementById.

  //The nested functions are needed because doOnOk cannot access anything outside of its scope
  //when it is called 
  function getCheckboxValue(name){
    if ("doAllowWindowOpen" in window)
      return document.getElementById(name).checked;
    
    return data[name].checked;
  }
 
  function setCapabilityPolicy(prefName, checkboxValue){

    //If checked, we allow the script to do task, so we clear the pref.
    //since some options are made up of multiple capability policies and users can turn 
    //individual ones on/off via prefs.js, it can happen that we clear a nonexistent pref
    if (checkboxValue){
      try { 
        parent.hPrefWindow.pref.ClearUserPref(prefName);
      } catch (e) {}
    } else {
      parent.hPrefWindow.setPref("string", prefName, "noAccess");
    }
  }

  var data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-scripts.xul"];   
 
  if (data.scriptData["allowWindowOpenChanged"].value){
    parent.hPrefWindow.setPref("bool", "dom.disable_open_during_load", 
      !getCheckboxValue('allowWindowOpen'));
  }

  if (data.scriptData["allowWindowMoveResizeChanged"].value){
    var myValue = getCheckboxValue("allowWindowMoveResize");

    setCapabilityPolicy("capability.policy.default.Window.resizeTo", myValue);
    setCapabilityPolicy("capability.policy.default.Window.innerWidth.set", myValue);
    setCapabilityPolicy("capability.policy.default.Window.innerHeight.set", myValue);
    setCapabilityPolicy("capability.policy.default.Window.outerWidth.set", myValue);
    setCapabilityPolicy("capability.policy.default.Window.outerHeight.set", myValue);
    setCapabilityPolicy("capability.policy.default.Window.sizeToContent", myValue);
    setCapabilityPolicy("capability.policy.default.Window.resizeBy", myValue);
    setCapabilityPolicy("capability.policy.default.Window.screenX.set", myValue);
    setCapabilityPolicy("capability.policy.default.Window.screenY.set", myValue);
    setCapabilityPolicy("capability.policy.default.Window.moveTo", myValue);
    setCapabilityPolicy("capability.policy.default.Window.moveBy", myValue);
  }

  if (data.scriptData["allowWindowStatusChangeChanged"].value){
    setCapabilityPolicy("capability.policy.default.Window.status", 
      getCheckboxValue("allowWindowStatusChange"));
  }
  
  if (data.scriptData["allowWindowFlipChanged"].value){
    setCapabilityPolicy("capability.policy.default.Window.focus", 
      getCheckboxValue("allowWindowFlip"));
  }

  if (data.scriptData["allowDocumentCookieSetChanged"].value){
    setCapabilityPolicy("capability.policy.default.HTMLDocument.cookie.set", 
      getCheckboxValue("allowDocumentCookieSet"));
  }  

  if (data.scriptData["allowDocumentCookieGetChanged"].value){
    setCapabilityPolicy("capability.policy.default.HTMLDocument.cookie.get", 
      getCheckboxValue("allowDocumentCookieGet"));
  } 

  if (data.scriptData["allowImageSrcChangeChanged"].value){
    setCapabilityPolicy("capability.policy.default.HTMLImageElement.src", 
      getCheckboxValue("allowImageSrcChange"));
  }
}
