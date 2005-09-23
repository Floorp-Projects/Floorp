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
 *   Marcio S. Galli - mgalli@geckonnection.com
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
var gtabCounter=0;
var gBrowserStatusHandler;
var gSelectedTab=null;
var gFullScreen=false;
var gRSSTag="minimo";
var gGlobalHistory = null;
var gURIFixup = null;

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
    this.stopreloadButton = document.getElementById("reload-stop-button");
    this.progressBGPosition = 0;  /* To be removed, fix in onProgressChange ... */ 
  },

  destroy : function()
  {
    this.urlBar = null;
    this.stopreloadButton = null;
    this.progressBGPosition = null;  /* To be removed, fix in onProgressChange ... */ 
    
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    var refBrowser=null;
    var tabItem=null;
    
    if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK)
    {

      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        this.stopreloadButton.className = "stop-button";
        this.stopreloadButton.onClick = "BrowserStop()";
        
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
      
        /* Find this Browser and this Tab */ 
        
        if (getBrowser().mTabbedMode) {
          for (var i = 0; i < getBrowser().mPanelContainer.childNodes.length; i++) {
            if(getBrowser().getBrowserAtIndex(i).contentDocument==aWebProgress.DOMWindow.document) {				
              refBrowser=getBrowser().getBrowserAtIndex(i);
              tabItem=getBrowser().mTabContainer.childNodes[i];
			} 
          }
        }
        
        /* Apply RSS fetch if the request type is for rss view. 
           Gotta fix this to use a better approach.  */
        
        const rsschromemask = "rssblank";
        if(aRequest.name.indexOf(rsschromemask)>-1) {
          rssfetch(tabItem,refBrowser.contentDocument,refBrowser.contentDocument.body);
        }
      
        /* To be fixed. We dont want to directly access sytle from here */
        document.styleSheets[1].cssRules[0].style.backgroundPosition="1000px 100%";

        this.stopreloadButton.className = "reload-button";
        this.stopreloadButton.onClick = "BrowserReload()";
        
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
    var percentage = parseInt((aCurTotalProgress * 100) / aMaxTotalProgress);
    document.styleSheets[1].cssRules[0].style.backgroundPosition=percentage+"px 100%";
  },
  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
     /* Ideally we dont want to check this here.
     Better to have some other protocol view-rss in the chrome */

     const rsschromemask = "chrome://minimo/content/rssview/rssblank.xhtml";

     if(aLocation.spec.substr(0, rsschromemask.length) == rsschromemask) {
        this.urlBar.value="SB: "+gRSSTag; 
     } else {
      domWindow = aWebProgress.DOMWindow;
      // Update urlbar only if there was a load on the root docshell
      if (domWindow == domWindow.top) {
        this.urlBar.value = aLocation.spec;
      }
    }
},

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
    /* Color is temporary. We shall dynamically assign a new class to the element and or to 
    evaluate access from another class rule, the security identity color has to be with the minimo.css */ 

    switch (aState) {
      case nsIWebProgressListener.STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_HIGH:
        //this.urlBar.value="level high";
        document.styleSheets[1].cssRules[0].style.backgroundColor="yellow";
        document.getElementById("lock-icon").className="security-notbroken";
        break;	
      case nsIWebProgressListener.STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_LOW:
        // this.urlBar.value="level low";
        document.styleSheets[1].cssRules[0].style.backgroundColor="lightyellow";
        document.getElementById("lock-icon").className="security-notbroken";
        break;
      case nsIWebProgressListener.STATE_IS_BROKEN:
        //this.urlBar.value="level broken";
        document.styleSheets[1].cssRules[0].style.backgroundColor="lightred";
        document.getElementById("lock-icon").className="security-broken";
        break;
      case nsIWebProgressListener.STATE_IS_INSECURE:
        default:
        document.styleSheets[1].cssRules[0].style.backgroundColor="white";
        document.getElementById("lock-icon").className="security-na";
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
  var homepage = "http://www.mozilla.org";

  try {

    gURLBar = document.getElementById("urlbar");
    var currentTab=getBrowser().selectedTab;
    browserInit(currentTab);
    gSelectedTab=currentTab;
    
    var BrowserStatusHandler = new nsBrowserStatusHandler();
    BrowserStatusHandler.init();

    getBrowser().addProgressListener(BrowserStatusHandler, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  
    // Current build was not able to get it. Taking it from the tab browser element. 
    // var webNavigation=getBrowser().webNavigation;

    var refBrowser=getBrowser().getBrowserForTab(currentTab);
    var webNavigation=refBrowser.webNavigation;
    
    webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"].createInstance(Components.interfaces.nsISHistory);

    getBrowser().docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).useGlobalHistory = true;

    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                               .getService(Components.interfaces.nsIBrowserHistory);
  
    gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                          .getService(Components.interfaces.nsIURIFixup);


    try {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);

      var page = pref.getCharPref("browser.startup.homepage");

      if (page != null)
      {
        var fixedUpURI = gURIFixup.createFixupURI(page, 2 /*fixup url*/ );
        homepage = fixedUpURI.spec;
      }
    } catch (ignore) {}
      
  } catch (e) {
    alert("Error trying to startup browser.  Please report this as a bug:\n" + e);
  }

  loadURI(homepage);
}

/** 
  * Init stuff
  * 
  **/
function browserInit(refTab)
{
  var refBrowser=getBrowser().getBrowserForTab(refTab);
  try {
    refBrowser.markupDocumentViewer.textZoom = .90;
  } catch (e) {
  
  }
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


/**
  * Has to go through some other approach like a XML-based rule system. 
  * Those are constraints conditions and action. 
  **/
  
function BrowserViewOptions() {
  document.getElementById("toolbar-view").collapsed=!document.getElementById("toolbar-view").collapsed;
  if(document.getElementById("toolbar-view").collapsed &&  document.getElementById("command_ViewOptions").getAttribute("checked")=="true") {
	document.getElementById("command_ViewOptions").setAttribute("checked","false");
  }
}

/**
  * Has to go through some other approach like a XML-based rule system. 
  * Those are constraints conditions and action. 
  **/
  
function BrowserViewRSS() {
  document.getElementById("toolbar-rss").collapsed=!document.getElementById("toolbar-rss").collapsed;
  if(document.getElementById("toolbar-rss").collapsed &&  document.getElementById("command_ViewRSS").getAttribute("checked")=="true") {
	document.getElementById("command_ViewRSS").setAttribute("checked","false");
  }
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

function DoBrowserRSS() {
  BrowserViewRSS();
  try { 
    if(document.getElementById("toolbar-rss-rsstag").value!="") {
      gRSSTag=document.getElementById("toolbar-rss-rsstag").value;
    }
    getBrowser().selectedTab = getBrowser().addTab('chrome://minimo/content/rssview/rssblank.xhtml');
    browserInit(getBrowser().selectedTab);
  } catch (e) {
   
  }  
}

function DoPanelPreferences() {
  window.openDialog("chrome://minimo/content/preferences.xul","preferences","modal,centerscreeen,chrome,resizable=no");
  BrowserReload(); 
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

function URLBarEntered()
{
  try
  {
    var url = gURLBar.value;
    
    var fixedUpURI = gURIFixup.createFixupURI(url, 2 /*fixup url*/ );
    gGlobalHistory.markPageAsTyped(fixedUpURI);
    
    gURLBar.value = fixedUpURI.spec;
    
    loadURI(fixedUpURI.spec);
    
    content.focus();
  }
  catch(ex) {alert(ex);}


  return true;
}

function URLBarFocusHandler()
{
  gURLBar.showHistoryPopup();
}

var gRotationDirection = true;

function BrowserScreenRotate()
{
  try {
  var deviceSupport = Components.classes["@mozilla.org/device/support;1"].getService(Components.interfaces.nsIDeviceSupport);
  
  deviceSupport.rotateScreen(gRotationDirection);
  gRotationDirection != gRotationDirection;
  }
  catch (ex)
  {
    alert(ex);
  }
}
