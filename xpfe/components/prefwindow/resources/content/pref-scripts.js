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
 * Doron Rosenerg.
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

var allowWindowOpenChanged = false;
var allowWindowMoveResizeChanged = false;
var allowWindowFlipChanged = false;
var allowWindowStatusChangeChanged = false;
var allowImageSrcChangeChanged = false;
var allowDocumentCookieSetChanged = false;
var allowDocumentCookieGetChanged = false;

function setCapabilityPolicy(prefName, checkboxValue){

 //if checked, we allow the script to do task, so we clear the pref.
 //since some options are made up of multiple capability policies and users can turn 
 //individual ones on/off via prefs.js, it can happen that we clear a nonexistent pref
  if (checkboxValue){
    try { 
      pref.ClearUserPref(prefName);
    } catch (e) {}
  } else {
    pref.SetCharPref(prefName, "noAccess");
  }
}

function doAllowWindowOpen(){
  allowWindowOpenChanged = !allowWindowOpenChanged; 
}

function doWindowMoveResize(){
  allowWindowMoveResizeChanged =  !allowWindowMoveResizeChanged;
}

function doWindowStatusChange(){
  allowWindowStatusChangeChanged = !allowWindowStatusChangeChanged;
}

function doWindowFlipChange(){
  allowWindowFlipChanged = !allowWindowFlipChanged;
}

function doAllowCookieSet(){
  allowDocumentCookieSetChanged = !allowDocumentCookieSetChanged;
}

function doAllowCookieGet(){
  allowDocumentCookieGetChanged =  !allowDocumentCookieGetChanged;  
}

function doAllowImageSrcChange(){
  allowImageSrcChangeChanged = !allowImageSrcChangeChanged;
}

function changeDisabledState(state){
  // set the groupbox children's state based on the "javascript enabled" checkbox value
  document.getElementById('allowScriptsDescription').disabled = state;
  document.getElementById('allowWindowMoveResize').disabled = state;
  document.getElementById('allowWindowOpen').disabled = state;
  document.getElementById('allowImageSrcChange').disabled = state;
  document.getElementById('allowDocumentCookieSet').disabled = state;
  document.getElementById('allowDocumentCookieGet').disabled = state;
  document.getElementById('allowWindowStatusChange').disabled = state;
  document.getElementById('allowWindowFlip').disabled = state;
}

function javascriptEnabledChange(state){
  changeDisabledState(!state);
}

function getPrefValueForCheckbox(prefName){

  var prefValue;
  var rv = false;

  try {
    prefValue = pref.GetCharPref(prefName);
    
    if(prefValue == "allAccess" || prefValue == "sameOrigin"){
      rv = true; 
    }
  }
  catch(e) { //if no pref set, check the box, as it is equivalent with "AllAccess" or "sameOrigin"
    rv = true;
  }

  return rv;
}

function Startup(){

  try{
    document.getElementById('allowWindowOpen').checked = !pref.GetBoolPref("dom.disable_open_during_load");
  } catch (e){
    //if we get an error, the pref is not existent, we default to true
    document.getElementById('allowWindowOpen').checked = true;
  }

  //if one of the security capability prefs is set, checkbox becomes unchecked
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

  //if javascript is disabled, disable the checkboxes
  if (!document.getElementById('javascriptEnabled').checked) changeDisabledState(true);

  parent.hPrefWindow.registerOKCallbackFunc(doOnOk);
}

function doOnOk(){
 
  if (allowWindowOpenChanged){
    pref.SetBoolPref("dom.disable_open_during_load", !document.getElementById("allowWindowOpen").checked);
  }

  if (allowWindowMoveResizeChanged){
    var myValue = document.getElementById("allowWindowMoveResize").checked;

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

  if (allowWindowStatusChangeChanged){
    setCapabilityPolicy("capability.policy.default.Window.status", 
      document.getElementById("allowWindowStatusChange").checked);
  }
  
  if (allowWindowFlipChanged){
    setCapabilityPolicy("capability.policy.default.Window.focus", 
      document.getElementById("allowWindowFlip").checked);
  }

  if (allowDocumentCookieSetChanged){
    setCapabilityPolicy("capability.policy.default.HTMLDocument.cookie.set", 
      document.getElementById("allowDocumentCookieSet").checked);
  }  

  if (allowDocumentCookieGetChanged){
    setCapabilityPolicy("capability.policy.default.HTMLDocument.cookie.get", 
      document.getElementById("allowDocumentCookieGet").checked);
  } 

  if (allowImageSrcChangeChanged){
    setCapabilityPolicy("capability.policy.default.HTMLImageElement.src", 
      document.getElementById("allowImageSrcChange").checked);
  }
}
