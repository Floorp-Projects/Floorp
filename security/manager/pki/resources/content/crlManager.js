/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsICRLManager = Components.interfaces.nsICRLManager;
const nsCRLManager  = "@mozilla.org/security/crlmanager;1";
const nsICRLInfo = Components.interfaces.nsICRLInfo;
const nsISupportsArray = Components.interfaces.nsISupportsArray;
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsPKIParamBlock    = "@mozilla.org/security/pkiparamblock;1";
const nsIPrefService      = Components.interfaces.nsIPrefService;

var crlManager;
var crls;
var prefService;
var prefBranch;

var autoupdateEnabledBaseString   = "security.crl.autoupdate.enable.";
var autoupdateTimeTypeBaseString  = "security.crl.autoupdate.timingType.";
var autoupdateTimeBaseString      = "security.crl.autoupdate.nextInstant.";
var autoupdateURLBaseString       = "security.crl.autoupdate.url.";
var autoupdateErrCntBaseString    = "security.crl.autoupdate.errCount.";
var autoupdateErrDetailBaseString = "security.crl.autoupdate.errDetail.";
var autoupdateDayCntString        = "security.crl.autoupdate.dayCnt.";
var autoupdateFreqCntString       = "security.crl.autoupdate.freqCnt.";

function onLoad()
{
  var crlEntry;
  var i;

  crlManager = Components.classes[nsCRLManager].getService(nsICRLManager);
  crls = crlManager.getCrls();
  prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefService);
  prefBranch = prefService.getBranch(null);
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var autoupdateEnabledString;
  var autoupdateErrCntString;

  for (i=0; i<crls.length; i++) {
    crlEntry = crls.queryElementAt(i, nsICRLInfo);
    var org = crlEntry.organization;
    var orgUnit = crlEntry.organizationalUnit;
    var lastUpdate = crlEntry.lastUpdateLocale;
    var nextUpdate = crlEntry.nextUpdateLocale;
    autoupdateEnabledString    = autoupdateEnabledBaseString + crlEntry.nameInDb;
    autoupdateErrCntString    = autoupdateErrCntBaseString + crlEntry.nameInDb;
    var enabled = false;
    var enabledStr = bundle.GetStringFromName("crlAutoupdateNotEnabled");
    var status = "";
    try{
      enabled = prefBranch.getBoolPref(autoupdateEnabledString)
      if(enabled){
        enabledStr = bundle.GetStringFromName("crlAutoupdateEnabled");
      }
      var cnt;
      cnt = prefBranch.getIntPref(autoupdateErrCntString);
      if(cnt > 0){
        status = bundle.GetStringFromName("crlAutoupdateFailed");
      } else {
        status = bundle.GetStringFromName("crlAutoupdateOk");
      }
    }catch(exception){}
    
    AddItem("crlList", [org, orgUnit, lastUpdate, nextUpdate, enabledStr, status], "crltree_", i);
  }
}

function AddItem(children,cells,prefix,idfier)
{
  var kids = document.getElementById(children);
  var item  = document.createElement("treeitem");
  var row   = document.createElement("treerow");
  for(var i = 0; i < cells.length; i++)
  {
    var cell  = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("label", cells[i])
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id",prefix + idfier);
  kids.appendChild(item);
}

function DeleteCrlSelected() {
  var crlEntry;

  // delete selected item
  var crltree = document.getElementById("crltree");
  var i = crltree.currentIndex;
  if(i<0){
    return;
  }
  crlEntry = crls.queryElementAt(i, nsICRLInfo);
    
  var autoupdateEnabled = false;
  var autoupdateParamAvailable = false;
  var id = crlEntry.nameInDb;
  
  //First, check if autoupdate was enabled for this crl
  try {
    autoupdateEnabled = prefBranch.getBoolPref(autoupdateEnabledBaseString + id);
    //Note, if the pref is not present, we get an exception right here,
    //and autoupdateEnabled remains false
    autoupdateParamAvailable = true;
    prefBranch.clearUserPref(autoupdateEnabledBaseString + id);
    prefBranch.clearUserPref(autoupdateTimeTypeBaseString + id);
    prefBranch.clearUserPref(autoupdateTimeBaseString + id);
    prefBranch.clearUserPref(autoupdateURLBaseString + id);
    prefBranch.clearUserPref(autoupdateDayCntString + id);
    prefBranch.clearUserPref(autoupdateFreqCntString + id);
    prefBranch.clearUserPref(autoupdateErrCntBaseString + id);
    prefBranch.clearUserPref(autoupdateErrDetailBaseString + id);
  } catch(Exception){}

  //Once we have deleted the prefs that can be deleted, we save the 
  //file if relevant, restart the scheduler, and once we are successful 
  //in doind that, we try to delete the crl 
  try{
    if(autoupdateParamAvailable){
      prefService.savePrefFile(null);
    }

    if(autoupdateEnabled){
      crlManager.rescheduleCRLAutoUpdate();
    }
          
    // Now, try to delete it
    crlManager.deleteCrl(i);
    DeleteItemSelected("crltree", "crltree_", "crlList");
    //To do: If delete fails, we should be able to retrieve the deleted
    //settings
    //XXXXXXXXXXXXXXXXXXXXX
  
  }catch(exception) {
    //To Do: Possibly show an error ...
    //XXXXXXXXXXXX
  }

  EnableCrlActions();
}

function EnableCrlActions() {
  var tree = document.getElementById("crltree");
  if (tree.view.selection.count) {
    document.getElementById("deleteCrl").removeAttribute("disabled");
    document.getElementById("editPrefs").removeAttribute("disabled");
    document.getElementById("updateCRL").removeAttribute("disabled");
  } else {
    document.getElementById("deleteCrl").setAttribute("disabled", "true");
    document.getElementById("editPrefs").setAttribute("disabled", "true");
    document.getElementById("updateCRL").setAttribute("disabled", "true");
  }
}

function DeleteItemSelected(tree, prefix, kids) {
  var i;
  var delnarray = [];
  var rv = "";
  var cookietree = document.getElementById(tree);
  var rangeCount = cookietree.view.selection.getRangeCount();
  for(i = 0; i < rangeCount; ++i) 
  { 
    var start = {}, end = {};
    cookietree.view.selection.getRangeAt(i, start, end);
    for (var k = start.value; k <= end.value; ++k) {
      var item = cookietree.contentView.getItemAtIndex(k);
      delnarray[i] = document.getElementById(item.id);
      var itemid = parseInt(item.id.substring(prefix.length, item.id.length));
      rv += (itemid + ",");
    }
  }
  for(i = 0; i < delnarray.length; i++) 
  { 
    document.getElementById(kids).removeChild(delnarray[i]);
  }
  return rv;
}

function EditAutoUpdatePrefs() {
  var crlEntry;

  // delete selected item
  var crltree = document.getElementById("crltree");
  var i = crltree.currentIndex;
  if(i<0){
    return;
  }
  crlEntry = crls.queryElementAt(i, nsICRLInfo);
  var params = Components.classes[nsPKIParamBlock].createInstance(nsIPKIParamBlock);
  params.setISupportAtIndex(1, crlEntry);
  window.openDialog("chrome://pippki/content/pref-crlupdate.xul","",
                    "chrome,centerscreen,modal", params);
}

function UpdateCRL()
{
  var crlEntry;
  var crltree = document.getElementById("crltree");
  var i = crltree.currentIndex;
  if(i<0){
    return;
  }
  crlEntry = crls.queryElementAt(i, nsICRLInfo);
  crlManager.updateCRLFromURL(crlEntry.lastFetchURL, crlEntry.nameInDb);
}

function ImportCRL()
{
  // prompt for the URL to import from
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  
  var CRLLocation = {value:null};
  var dummy = { value: 0 };
  var strBundle = document.getElementById('pippki_bundle');
  var addCRL = promptService.prompt(window, strBundle.getString('crlImportNewCRLTitle'), 
                                    strBundle.getString('crlImportNewCRLLabel'),  CRLLocation, null, dummy);

  if (addCRL)
    crlManager.updateCRLFromURL(CRLLocation.value, "");
}
