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
 *   Jonas Jørgensen <jonasj@jonasj.dk>
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

function changeDisabledState(state){
  //Set the states of the groupbox children state based on the "javascript enabled" checkbox value
  document.getElementById("allowScripts").disabled = state;
  document.getElementById("allowWindowMoveResize").disabled = state;
  document.getElementById("allowImageSrcChange").disabled = state;
  document.getElementById("allowWindowStatusChange").disabled = state;
  document.getElementById("allowWindowFlip").disabled = state;
  document.getElementById("allowHideStatusBar").disabled = state;
}

function javascriptEnabledChange(){
  // if javascriptAllowMailNews is overlayed (mailnews is installed), then if javascriptAllowMailnews 
  // and javascriptAllowNavigator are unchecked, we disable the tree items. 
  // If javascriptAllowMailNews is not available, we only take javascriptAllowNavigator in consideration

  if (document.getElementById('javascriptAllowMailNews')){
    if (!document.getElementById('javascriptAllowNavigator').checked && !document.getElementById('javascriptAllowMailNews').checked)
      changeDisabledState(true);
    else changeDisabledState(false);
  } else {
    changeDisabledState(!document.getElementById('javascriptAllowNavigator').checked);
  }
}

function getPrefValueForCheckbox(prefName){
  var prefValue = false;

  try {
    prefValue = pref.GetBoolPref(prefName);
  }
  catch(e) {}

  // the prefs are stored in terms of disabling,
  // but we want our value in terms of enabling.
  // so let's invert the prefValue.
  return !prefValue;
}

function Startup(){

  data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-scripts.xul"];

  //If scriptData does not exist, then it is the first time the panel was shown and we default to false 
  if (!("scriptData" in data)){
    var changedList = ["allowWindowMoveResizeChanged",
                       "allowWindowStatusChangeChanged",
                       "allowWindowFlipChanged",
                       "allowImageSrcChangeChanged",
                       "allowHideStatusBarChanged"];

    data.scriptData = [];
    for(var run = 0; run < changedList.length; run++ ){
      data.scriptData[ changedList[run] ] = [];
      data.scriptData[ changedList[run] ].value = false;
    }

    document.getElementById("allowWindowMoveResize").checked =  getPrefValueForCheckbox("dom.disable_window_move_resize");
    document.getElementById("allowWindowFlip").checked = getPrefValueForCheckbox("dom.disable_window_flip");
    document.getElementById("allowWindowStatusChange").checked = getPrefValueForCheckbox("dom.disable_window_status_change");
    document.getElementById("allowImageSrcChange").checked = getPrefValueForCheckbox("dom.disable_image_src_set");
    document.getElementById("allowHideStatusBar").checked = getPrefValueForCheckbox("dom.disable_window_open_feature.status");

    //If we don't have a checkbox under groupbox pluginPreferences, we should hide it
    var pluginGroup = document.getElementById("pluginPreferences")
    var children = pluginGroup.childNodes;
    if (!children || children.length <= 1)    // 1 for the caption
      pluginGroup.setAttribute("hidden", "true");
  }

  javascriptEnabledChange();

  document.getElementById("AllowList").addEventListener("CheckboxStateChange", onCheckboxCheck, false);

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
    if ("onCheckboxCheck" in window)
      return document.getElementById(name).checked;

    return data[name].checked;
  }

  var data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-scripts.xul"];

  if (data.scriptData["allowWindowMoveResizeChanged"].value){
    parent.hPrefWindow.setPref("bool", "dom.disable_window_move_resize",
      !getCheckboxValue('allowWindowMoveResize'));
  }

  if (data.scriptData["allowWindowStatusChangeChanged"].value){
    parent.hPrefWindow.setPref("bool", "dom.disable_window_status_change",
      !getCheckboxValue("allowWindowStatusChange"));
  }

  if (data.scriptData["allowWindowFlipChanged"].value){
    parent.hPrefWindow.setPref("bool", "dom.disable_window_flip",
      !getCheckboxValue("allowWindowFlip"));
  }

  if (data.scriptData["allowImageSrcChangeChanged"].value){
    parent.hPrefWindow.setPref("bool", "dom.disable_image_src_set",
      !getCheckboxValue("allowImageSrcChange"));
  }

  if (data.scriptData["allowHideStatusBarChanged"].value) {
    parent.hPrefWindow.setPref("bool", "dom.disable_window_open_feature.status",
      !getCheckboxValue("allowHideStatusBar"));
  }
}

function onCheckboxCheck(event)
{
  data.scriptData[event.target.id+"Changed"].value = !data.scriptData[event.target.id+"Changed"].value;
}
