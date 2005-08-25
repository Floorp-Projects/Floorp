/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Marcio S. Galli - mgalli@geckonnection.com
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

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;

var gURLBar = null;
var gBrowserStatusHandlerArray=new Array();
var gtabCounter=0;
var gBrowserStatusHandler;
var gSelectedTab=null;
var gFullScreen=false;
var gGlobalHistory = null;
var gURIFixup = null;
var gSSRSupport = null;

function nsBrowserStatusHandler()
{
}

nsBrowserStatusHandler.prototype = 
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsISupports))
    {
      return this;
    }
    throw Components.results.NS_NOINTERFACE;
  },

  init : function()
  {
    this.urlBar           = document.getElementById("urlbar");
    this.statusbarText    = document.getElementById("statusbar-text");
    this.stopreloadButton = document.getElementById("reload-stop-button");
    this.statusbar        = document.getElementById("statusbar");
    this.progressBGPosition = 0;  /* To be removed, fix in onProgressChange ... */ 
    
  },

  destroy : function()
  {
    this.urlBar = null;
    this.statusbarText = null;
    this.stopreloadButton = null;
    this.statusbar = null;
    this.progressBGPosition = null;  /* To be removed, fix in onProgressChange ... */ 
    
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {

    if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK)
    {

      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        this.stopreloadButton.className = "stop-button";
        this.stopreloadButton.onClick = "BrowserStop()";
        this.statusbar.hidden = false;
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        /* To be fixed. We dont want to directly access sytle from here */
        document.styleSheets[1].cssRules[0].style.backgroundPosition="1000px 100%";

        this.stopreloadButton.className = "reload-button";
        this.stopreloadButton.onClick = "BrowserReload()";
        
        this.statusbar.hidden = true;
        this.statusbarText.label = "";
        return;
      }
      return;
    }

    if (aStateFlags & nsIWebProgressListener.STATE_IS_DOCUMENT)
    { 
      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        return;
      }
      return;
    }
  },

  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {
  
    /* This is the in-progress version of our background progress bar under urlbar */
    this.progressBGPosition+=3;
    document.styleSheets[1].cssRules[0].style.backgroundPosition=this.progressBGPosition+"px 0%";

    /* This is to be the new version, when the aCurTotalProgress aMaxTotalProgres values 
    are correct...
    
    var percentage = parseInt((aCurTotalProgress * 100) / aMaxTotalProgress);
    document.styleSheets[1].cssRules[0].style.backgroundPosition=percentage+"px 100%";

    */
    
  },
  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
        domWindow = aWebProgress.DOMWindow;
        // Update urlbar only if there was a load on the root docshell
        if (domWindow == domWindow.top) {
          this.urlBar.value = aLocation.spec;
        }
        this.refTab.setAttribute("lastLocation",aLocation.spec);
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    this.statusbarText.label = aMessage;
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
    /* Color is temporary. We shall dynamically assign a new class to the element and or to 
    evaluate access from another class rule, the security identity color has to be with the minimo.css */ 

    switch (aState) {
      case nsIWebProgressListener.STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_HIGH:
        //this.urlBar.value="level high";
        document.styleSheets[1].cssRules[0].style.backgroundColor="yellow";
        break;	
      case nsIWebProgressListener.STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_LOW:
        // this.urlBar.value="level low";
        document.styleSheets[1].cssRules[0].style.backgroundColor="lightyellow";
        break;
      case nsIWebProgressListener.STATE_IS_BROKEN:
        //this.urlBar.value="level broken";
        document.styleSheets[1].cssRules[0].style.backgroundColor="lightred";
        break;
      case nsIWebProgressListener.STATE_IS_INSECURE:
        default:
        document.styleSheets[1].cssRules[0].style.backgroundColor="white";
        break;
      }   
  }
}

/** 
  * Initial Minimo Startup 
  * 
  **/

/* moved this as global */ 


function MiniNavStartup()
{

  gURLBar = document.getElementById("urlbar");
  var currentTab=getBrowser().selectedTab;
  browserInit(currentTab);
  gSelectedTab=currentTab;
  loadURI("http://www.google.com/");
  

}

/** 
  * Init stuff
  * 
  **/
function browserInit(refTab)
{

    var BrowserStatusHandler = new nsBrowserStatusHandler();

    BrowserStatusHandler.init();
    BrowserStatusHandler.refTab=refTab; // WARNING (was refTab);

    try {

      getBrowser().addProgressListener(BrowserStatusHandler, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
      gBrowserStatusHandlerArray.push(BrowserStatusHandler);
      var refBrowser=getBrowser().getBrowserForTab(refTab);
      var webNavigation=refBrowser.webNavigation;
      webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"].createInstance(Components.interfaces.nsISHistory);
    } catch (e) {
      alert("Error trying to startup browser.  Please report this as a bug:\n" + e);
    }

    try {
      refBrowser.markupDocumentViewer.textZoom = .90;
    } catch (e) {}
    gURLBar = document.getElementById("urlbar");

}

function MiniNavShutdown()
{
  if (gBrowserStatusHandler) gBrowserStatusHandler.destroy();
}

function getBrowser()
{
  return document.getElementById("content");
}

function getWebNavigation()
{
  return getBrowser().webNavigation;
}

function loadURI(uri)
{
  getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
}

function BrowserLoadURL()
{
  try {
    loadURI(gURLBar.value);
    content.focus();
  }
  catch(e) {
  }
}

function BrowserBack()
{
  getWebNavigation().goBack();
}

function BrowserForward()
{
  getWebNavigation().goForward();
}

function BrowserStop()
{
  getWebNavigation().stop(nsIWebNavigation.STOP_ALL);
}

function BrowserReload()
{
  getWebNavigation().reload(nsIWebNavigation.LOAD_FLAGS_NONE);
}

function BrowserAddTab() 
{
    var thisTab=getBrowser().addTab("http://mozilla.org");
    browserInit(thisTab);
}

function BrowserOpenTab()
{
  try { 
    getBrowser().selectedTab = getBrowser().addTab('about:blank');
    browserInit(getBrowser().selectedTab);
  } catch (e) {
    alert(e);
  }
  //  if (gURLBar) setTimeout(function() { gURLBar.focus(); }, 0);

}


function BrowserViewOptions() {
	document.getElementById("toolbar-view").collapsed=!document.getElementById("toolbar-view").collapsed;
}


/** 
  * Work-in-progress, this handler is 100% generic and gets all the clicks for the entire tabbed 
  * content area. Need to fix with the approach where we handler only clicks for the actual tab. 
  * Need to fix to also handle the key action case. 
  **/
function tabbrowserAreaClick(e) {

    // When the click happens, 
    // updates the location bar with the lastLocation for the given selectedTab. 
    // Note that currently the lastLocation is stored in the DOM for the selectedTab. 

    gSelectedTab=getBrowser().selectedTab; 
    gURLBar.value=gSelectedTab.getAttribute("lastLocation");
    document.styleSheets[1].cssRules[0].style.backgroundPosition="1000px 100%";

}


/** 
  * urlbar indentity, style, progress indicator.
  **/ 
function urlbar() {
}


/* Reset the text size */ 
function BrowserResetZoomPlus() {
 	getBrowser().selectedBrowser.markupDocumentViewer.textZoom+= .25;
}

function BrowserResetZoomMinus() {
 	getBrowser().selectedBrowser.markupDocumentViewer.textZoom-= .25;
}

/* 
  We want to intercept before it shows, 
  to evaluate when the selected content area is a phone number, 
  thus mutate the popup menu to the right make call item 
*/ 
function BrowserPopupShowing () {
  var selectedRange=getBrowser().selectedBrowser.contentDocument.getSelection();
  document.getElementById("item-call").label="Call \""+ selectedRange + " \"";
  document.getElementById("item-call").setAttribute("oncommand","DoTestSendCall("+selectedRange+")");
  
  /* Enable Copy */
  if(selectedRange.toString()) {
    document.getElementById("item-copy").style.display="block";
  }
  
  /* Enable Paste - Can paste only if the focused element has a value attribute. :) 
    THis may affect XHTML nodes. Users may be able to paste things within XML nodes. 
  */
  if(document.commandDispatcher.focusedElement.nodeName=="INPUT"||document.commandDispatcher.focusedElement.nodeName=="TEXTAREA") {
    if(DoClipCheckPaste()) {
      document.getElementById("item-paste").style.display="block";	
    }
  }
}

/* 
  Testing the SMS and Call Services 
*/
function DoTestSendCall(toCall) {
  var phoneInterface= Components.classes["@mozilla.org/phone/support;1"].createInstance(Components.interfaces.nsIPhoneSupport);
  phoneInterface.makeCall(toCall,"");
}

function DoFullScreen()
{
  gFullScreen = !gFullScreen;
  
  document.getElementById("nav-bar").hidden = gFullScreen;
  document.getElementById("statusbar").hidden = gFullScreen;

  getBrowser().setStripVisibilityTo(!gFullScreen);
  window.fullScreen = gFullScreen;  
}

/* 

 */
function DoClipCopy()
{
  var copytext=getBrowser().selectedBrowser.contentDocument.getSelection().toString();
  var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);
  if (!str) return false;
  str.data = copytext;
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  if (!trans) return false;
  trans.addDataFlavor("text/unicode");
  trans.setTransferData("text/unicode",str,copytext.length * 2);
  var clipid = Components.interfaces.nsIClipboard;
  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].getService(clipid);
  if (!clip) return false;
  clip.setData(trans,null,clipid.kGlobalClipboard);
}

/* 
 Currently supports text/unicode. 
 */
function DoClipCheckPaste()
{
  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].getService(Components.interfaces.nsIClipboard);
  if (!clip) return false;
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  if (!trans) return false;
  trans.addDataFlavor("text/unicode");
  clip.getData(trans,clip.kGlobalClipboard);
  var str = new Object();
  var strLength = new Object();
  var pastetext = null;
  trans.getTransferData("text/unicode",str,strLength);
  if (str) str = str.value.QueryInterface(Components.interfaces.nsISupportsString);
  if (str) pastetext = str.data.substring(0,strLength.value / 2);
  if(pastetext) {
    return pastetext;
  } else return false;
}

function DoClipPaste()
{

  /* 008 note - value is there in the clipboard, however somehow paste action does not happen. 
   If you evaluate the canpaste you get false. */ 
   
  var disp = document.commandDispatcher;
  var cont = disp.getControllerForCommand("cmd_paste");
  cont.doCommand("cmd_paste");
}

function addToUrlbarHistory()
{
  var urlToAdd = gURLBar.value;
  
  if (!urlToAdd)
    return;
  
  if (urlToAdd.search(/[\x00-\x1F]/) != -1) // don't store bad URLs
    return;
  
  if (!gGlobalHistory)
    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                               .getService(Components.interfaces.nsIBrowserHistory);
  
  if (!gURIFixup)
    gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                          .getService(Components.interfaces.nsIURIFixup);
  try {
    if (urlToAdd.indexOf(" ") == -1) {
      var fixedUpURI = gURIFixup.createFixupURI(urlToAdd, 0);
       gGlobalHistory.markPageAsTyped(fixedUpURI);
    }
  }
  catch(ex) {
  }
}

function URLBarEntered()
{

  try {  addToUrlbarHistory(); } catch(ex) {}
  BrowserLoadURL();

  return true;
}

function URLBarFocusHandler()
{
  gURLBar.showHistoryPopup();
}

function URLBarClickHandler()
{
}

function BrowserToogleSSR()
{
  if(!gSSRSupport)
    gSSRSupport = Components.classes["@mozilla.org/ssr;1"].getService(Components.interfaces.nsISSRSupport);

  gSSRSupport.SSREnabled = !gSSRSupport.SSREnabled;

  BrowserReload(); // hack until this is done by ssr itself
}

function BrowserToogleSiteSSR()
{
  if(!gSSRSupport)
    gSSRSupport = Components.classes["@mozilla.org/ssr;1"].getService(Components.interfaces.nsISSRSupport);

  gSSRSupport.siteSSREnabled = !gSSRSupport.siteSSREnabled;

  BrowserReload(); // hack until this is done by ssr itself
}

