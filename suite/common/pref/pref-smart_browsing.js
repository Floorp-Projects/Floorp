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
 * The Original Code is Mozilla Communicator client code, released
 * August 15, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corp.  Portions created by Netscape Communications
 * Corp. are Copyright (C) 2001, Netscape Communications Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *      Diego Biurrun   <diego@biurrun.de>
 */

function Startup()
{
  // populate listbox
  var domainPrefField = document.getElementById( "browserRelatedDisabledForDomains" );
  var domains = domainPrefField.getAttribute("value").split(",");
  if (domains[0] != "")
    {
      for (var i = 0; i < domains.length; i++)
        createCell( domains[i] );
    }
    
  // select the first item    
  selectFirstCell();
  
  // enable buttons
  doButtonEnabling();
}

function selectFirstCell()
{
  var domainList = document.getElementById( "disabledDomains" );
  if (domainList.childNodes.length >= 1) {
    domainList.selectItem( domainList.firstChild );
  }
}
  
function addDomain()
{
  var domainField = document.getElementById( "addDomain" );
  if (domainField.value != "") {
    var domainList = document.getElementById( "disabledDomains" );
    domainList.selectItem( createCell( domainField.value ) );
    rebuildPrefValue();
    domainField.value = "";
    doButtonEnabling();
  }
}
  
    
function removeDomain()
{
  var domainList = document.getElementById( "disabledDomains" );
  var selectedItems = domainList.selectedItems;
  if (selectedItems.length >= 1)
  {
    for (var i = 0; i < selectedItems.length; i++)
    {
      domainList.removeChild( selectedItems[i] );
    }
  }
  selectFirstCell();
  rebuildPrefValue();
  toggleRemoveButton();
}
    
function rebuildPrefValue()
{
  var domainList = document.getElementById( "disabledDomains" );
  var string = "";
  if (domainList.hasChildNodes())
  {
    for (var i = 0; i < domainList.childNodes.length; i++)
    {
      var domain = domainList.childNodes[i].getAttribute("label");
      string += ( domain + "," );
    }
  }
  var domainPrefField = document.getElementById( "browserRelatedDisabledForDomains" );
  domainPrefField.setAttribute("value",string);
}
  
function createCell( aLabel )
{
  var domainList  = document.getElementById( "disabledDomains" );
  var item      = document.createElement( "listitem" );
  item.setAttribute( "label", aLabel );
  domainList.appendChild( item );
  return item;
}

function listboxHandleEvent( aEvent )
{
  if (aEvent.keyCode == 46)
    removeDomain();
}

function doButtonEnabling()
{
  var addDomain = document.getElementById("addDomain");
  var addDomainButton = document.getElementById("addDomainButton");
  var prefstring = document.getElementById("browserRelatedDisabledForDomains").getAttribute("value");
  if( addDomain.value == "" || prefstring.indexOf( addDomain.value + "," ) != -1 )
    addDomainButton.disabled = true;
  else
    addDomainButton.removeAttribute("disabled");
  if (parent.hPrefWindow.getPrefIsLocked(addDomainButton.getAttribute("prefstring")))
    addDomainButton.disabled = true;
  if (parent.hPrefWindow.getPrefIsLocked(addDomain.getAttribute("prefstring")))
    addDomain.disabled = true;
    
  toggleRemoveButton();
  toggleAutoCompleteAdvancedButton();
}  

function toggleRemoveButton()
{
  var domainList = document.getElementById("disabledDomains");
  var removeButton = document.getElementById("removeDomain");
  if (domainList.childNodes.length == 0)
    removeButton.disabled = true;
  else
    removeButton.disabled = false; 
}

function moreInfo()
{
  var browserURL = null;
  var regionBundle = document.getElementById("bundle_region");
  var smartBrowsingURL = regionBundle.getString("smartBrowsingURL");
  if (smartBrowsingURL) {
    try {
      var prefs = Components.classes["@mozilla.org/preferences;1"];
      if (prefs) {
        prefs = prefs.getService();
        if (prefs)
          prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
      }
      if (prefs) {
        var url = prefs.CopyCharPref("browser.chromeURL");
        if (url)
          browserURL = url;
      }
    } catch(e) {
    }
    if (browserURL == null)
      browserURL = "chrome://navigator/content/navigator.xul";
    window.openDialog( browserURL, "_blank", "chrome,all,dialog=no", smartBrowsingURL );
  }
}
   
function showACAdvanced()
{
  window.openDialog("chrome://communicator/content/pref/pref-smart_browsing-ac.xul", "", 
                    "modal=yes,chrome,resizable=no",
                    document.getElementById("browserUrlbarAutoFill").getAttribute("value"),
                    document.getElementById("browserUrlbarShowPopup").getAttribute("value"),
                    document.getElementById("browserUrlbarShowSearch").getAttribute("value"),
                    document.getElementById("browserUrlbarMatchOnlyTyped").getAttribute("value"));
}

function receiveACPrefs(aAutoFill, aShowPopup, aShowSearch, aAutoType)
{
  document.getElementById("browserUrlbarAutoFill").setAttribute("value", aAutoFill);
  document.getElementById("browserUrlbarShowPopup").setAttribute("value", aShowPopup);
  document.getElementById("browserUrlbarShowSearch").setAttribute("value", aShowSearch);
  document.getElementById("browserUrlbarMatchOnlyTyped").setAttribute("value", aAutoType);
}

 
function toggleAutoCompleteAdvancedButton()
{
  var browserAutoCompleteEnabled = document.getElementById("browserAutoCompleteEnabled");
  var autoCompleteAdvancedButton = document.getElementById("autoCompleteAdvancedButton");
  autoCompleteAdvancedButton.disabled = !browserAutoCompleteEnabled.checked;
}
