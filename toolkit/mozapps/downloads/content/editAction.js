# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

var gRDF, gItemRes, gHelperApps;
var gNC_URI;
var gHandlerRes, gExtAppRes;

var gLastSelectedActionItem = null;

function init()
{
  gRDF = window.opener.gRDF;
  gItemRes = window.arguments[0];
  
  gHelperApps = window.opener.gHelperApps;
  gHandlerPropArc = gHelperApps._handlerPropArc;
  gExternalAppArc = gHelperApps._externalAppArc;
  gFileHandlerArc = gHelperApps._fileHandlerArc;
  
  gNC_URI = window.opener.NC_URI;
  
  var typeField = document.getElementById("typeField");
  var str = gHelperApps.getLiteralValue(gItemRes.Value, "FileType");
  // XXXben localize!
  str += " (" + gHelperApps.getLiteralValue(gItemRes.Value, "FileExtensions") + ")";
  typeField.value = str;
  var typeIcon = document.getElementById("typeIcon");
  typeIcon.src = gHelperApps.getLiteralValue(gItemRes.Value, "FileIcon");

  var handlerGroup = document.getElementById("handlerGroup");
  
  gHandlerRes = gHelperApps.GetTarget(gItemRes, gHandlerPropArc, true);
  if (gHandlerRes) {
    gHandlerRes = gHandlerRes.QueryInterface(Components.interfaces.nsIRDFResource);

    // Custom App Handler Path - this must be set before we set the selected
    // radio button because the selection event handler for the radio group
    // requires the extapp handler field to be non-empty for the extapp radio
    // button to be selected. 
    var gExtAppRes = gHelperApps.GetTarget(gHandlerRes, gExternalAppArc, true);
    if (gExtAppRes) {
      gExtAppRes = gExtAppRes.QueryInterface(Components.interfaces.nsIRDFResource);

      var customAppPath = document.getElementById("customAppPath");          
      customAppPath.value = gHelperApps.getLiteralValue(gExtAppRes.Value, "path");
    }
    
    // Selected Action Radiogroup
    var handleInternal = gHelperApps.getLiteralValue(gHandlerRes.Value, "useSystemDefault");
    var saveToDisk = gHelperApps.getLiteralValue(gHandlerRes.Value, "saveToDisk");
    if (handleInternal == "true")
      handlerGroup.selectedItem = document.getElementById("openDefault");
    else if (saveToDisk == "true")
      handlerGroup.selectedItem = document.getElementById("saveToDisk");
    else
      handlerGroup.selectedItem = document.getElementById("openApplication");
      
    gLastSelectedActionItem = handlerGroup.selectedItem;
  }
  else {
    // No Handler/ExtApp Resources for this type for some reason
    handlerGroup.selectedItem = document.getElementById("openDefault");
  }

  var defaultAppName = document.getElementById("defaultAppName");
  var mimeInfo = gHelperApps.getMIMEInfo(gItemRes);
  defaultAppName.value = mimeInfo.defaultDescription;
  
  handlerGroup.focus();
  
  // We don't let users open .exe files or random binary data directly 
  // from the browser at the moment because of security concerns. 
  var mimeType = mimeInfo.MIMEType;
  if (mimeType == "application/object-stream" ||
      mimeType == "application/x-msdownload") {
    document.getElementById("openApplication").disabled = true;
    document.getElementById("openDefault").disabled = true;
    handlerGroup.selectedItem = document.getElementById("saveToDisk");
  }
  
  var x = document.documentElement.getAttribute("screenX");
  if (x == "")
    setTimeout(centerOverParent, 0);
}

function setLiteralValue(aResource, aProperty, aValue)
{
  var prop = gRDF.GetResource(gNC_URI(aProperty));
  var val = gRDF.GetLiteral(aValue);
  var oldVal = gHelperApps.GetTarget(aResource, prop, true);
  if (oldVal)
    gHelperApps.Change(aResource, prop, oldVal, val);
  else
    gHelperApps.Assert(aResource, prop, val, true);
}

function onAccept()
{
  var dirty = false;
  
  if (gHandlerRes) {  
    var handlerGroup = document.getElementById("handlerGroup");
    var value = handlerGroup.selectedItem.getAttribute("value");
    switch (value) {
    case "system":
      setLiteralValue(gHandlerRes, "saveToDisk", "false");
      setLiteralValue(gHandlerRes, "useSystemDefault", "true");
      break;
    case "app":
      setLiteralValue(gHandlerRes, "saveToDisk", "false");
      setLiteralValue(gHandlerRes, "useSystemDefault", "false");
      break;  
    case "save":
      setLiteralValue(gHandlerRes, "saveToDisk", "true");
      setLiteralValue(gHandlerRes, "useSystemDefault", "false");
      break;  
    }
    
    dirty = true;
  }
    
  if (gExtAppRes) {
    var customAppPath = document.getElementById("customAppPath");
    if (customAppPath.value != "") {
      setLiteralValue(gExtAppRes, "path", customAppPath.value);
      setLiteralValue(gExtAppRes, "prettyName", customAppPath.getAttribute("prettyName"));
    }
    
    dirty = true;
  }
  
  if (dirty) {
    gHelperApps.flush();
   
    // Get the template builder to refresh the display by announcing our imaginary
    // "FileHandler" property has been updated. (NC:FileHandler is a property
    // that our wrapper DS uses, its value is built dynamically based on real
    // values of other properties.
    var newHandler = gHelperApps.GetTarget(gItemRes, gFileHandlerArc, true);
    gHelperApps.onChange(gHelperApps, gItemRes, gFileHandlerArc, newHandler);
    
    // ... this doesn't seem to work, so...
    var fileHandlersList = window.opener.document.getElementById("fileHandlersList");
    fileHandlersList.builder.rebuild();
  }
    
  return true;
}

function centerOverParent()
{
  var parent = window.opener;
  
  x = parent.screenX + (parent.outerWidth / 2) - (window.outerWidth / 2);
  y = parent.screenY + (parent.outerHeight / 2) - (window.outerHeight / 2);
  window.moveTo(x, y);
}

function doEnabling(aSelectedItem)
{
  if (aSelectedItem.id == "openApplication") {
    var customAppPath = document.getElementById("customAppPath")
    customAppPath.disabled = false;
    document.getElementById("changeApp").disabled = false;
    
    if (customAppPath.value == "" && !changeApp()) {
      gLastSelectedActionItem.click();
      return;
    }
  }
  else {
    document.getElementById("customAppPath").disabled = true;
    document.getElementById("changeApp").disabled = true;
  }
  
  gLastSelectedActionItem = aSelectedItem;
}

function changeApp()
{
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);

  // extract the window title
  var winTitle = document.getElementById('changeApp').getAttribute('filepickertitle');
  fp.init(window, winTitle, nsIFilePicker.modeOpen);
  
  fp.appendFilters(nsIFilePicker.filterApps);
  if (fp.show() == nsIFilePicker.returnOK && fp.file) {
    var customAppPath = document.getElementById("customAppPath");
    customAppPath.value = fp.file.path;
    
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    var uri = ioService.newFileURI(fp.file);
    var url = uri.QueryInterface(Components.interfaces.nsIURL);
    customAppPath.setAttribute("prettyName", url.fileName);

    var mimeInfo = gHelperApps.getMIMEInfo(gItemRes);
    gExtAppRes = gRDF.GetResource("urn:mimetype:externalApplication:" + mimeInfo.MIMEType);
    
    
    return true;
  }
  
  return false;
}

