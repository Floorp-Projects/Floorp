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

const nsCI               = Components.interfaces;
const nsIWebNavigation = nsCI.nsIWebNavigation;
const nsIWebProgressListener = nsCI.nsIWebProgressListener;

var appCore = null;
var gBrowser = null;
var gBookmarksDoc=null; 
var gURLBar = null;
var gClickSelectsAll = true;
var gIgnoreFocus = false;
var gIgnoreClick = false;
var gBrowserStatusHandler;
var gSelectedTab=null;
var gFullScreen=false;
var gRSSTag="minimo";
var gGlobalHistory = null;
var gURIFixup = null;
var gShowingMenuPopup=false;
var gShowingNavMenuPopup=false;
var gFocusedElementHREFContextMenu=null;
var gDeckMode=0; // 0 = site, 1 = sb, 2= rss. Used for the URLBAR selector, DeckMode impl.
var gDeckMenuChecked=null; // to keep the state of the checked URLBAR selector mode. 

var gPref = null;                    // so far snav toggles on / off via direct access to pref.
                                     // See bugzilla.mozilla.org/show_bug.cgi?id=311287#c1
var gPrefAdded=false; // shall be used to flush the pref. 

function nsBrowserStatusHandler()
{
}

nsBrowserStatusHandler.prototype = 
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(nsCI.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
        aIID.equals(nsCI.nsISupportsWeakReference) ||
        aIID.equals(nsCI.nsISupports))
    {
      return this;
    }
    throw Components.results.NS_NOINTERFACE;
  },
  
  init : function()
  {
    this.urlBar           = document.getElementById("urlbar");
    this.progressBGPosition = 0;  /* To be removed, fix in onProgressChange ... */ 
  },
  
  destroy : function()
  {
    this.urlBar = null;
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
        // disable and hides the nav-menu-button; and enables unhide the stop button
		document.getElementById("nav-menu-button").hidden=true;
		document.getElementById("nav-stop-button").hidden=false;

        // Notify anyone interested that we are loading.
        try {
          var os = Components.classes["@mozilla.org/observer-service;1"]
                             .getService(Components.interfaces.nsIObserverService);
          var host = aRequest.QueryInterface(Components.interfaces.nsIChannel).URI.host;
          os.notifyObservers(null, "loading-domain", host);
        }
        catch(e) {}

        document.getElementById("statusbar").hidden=false;
        
		if(aRequest && aWebProgress.DOMWindow == content) {
          this.startDocumentLoad(aRequest);
		}
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        document.getElementById("statusbar").hidden=true;
        
        /* To be fixed. We dont want to directly access sytle from here */
        document.styleSheets[1].cssRules[0].style.backgroundPosition="1000px 100%";
        
        if (aRequest) {
            if (aWebProgress.DOMWindow == content) this.endDocumentLoad(aRequest, aStatus);
        }

        // disable and hides the nav-stop-button; and enables unhides the nav-menu-button button
		document.getElementById("nav-menu-button").hidden=true;
		document.getElementById("nav-stop-button").hidden=false;

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
        // 
        //        try {
        //          var imageCache = Components.classes["@mozilla.org/image/cache;1"]
        //                                   .getService(nsCI.imgICache);
        //          imageCache.clearCache(false);
        //        }
        //        catch(e) {}
        
        
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
    
    const rssmask = "chrome://minimo/content/rssview/rssload.xhtml?url=";
    const sbmask = "chrome://minimo/content/rssview/rssload.xhtml?url=http://del.icio.us/rss/tag/";
    
    if(aLocation.spec.substr(0, rssmask .length) == rssmask ) {
      
      if(aLocation.spec.substr(0, sbmask .length) == sbmask ) {
        /* We trap the URL */ 
        this.urlBar.value="sb:"+gRSSTag; 
        
      } else {
        
        /* We trap the URL */ 
        this.urlBar.value="rss:"+gRSSTag; 
        
      }
      
    } else {
      domWindow = aWebProgress.DOMWindow;
      // Update urlbar only if there was a load on the root docshell
      if (domWindow == domWindow.top) {
        this.urlBar.value = aLocation.spec;
      }
    }
    
    BrowserUpdateBackForwardState();
    
    BrowserUpdateFeeds();
  },
  
  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    document.getElementById("statusbar-text").label=aMessage;
  },
  startDocumentLoad : function(aRequest)
  {
    gBrowser.mCurrentBrowser.feeds = null;
  },
  endDocumentLoad : function(aRequest, aStatus)
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
  },
  
  setJSStatus : function(status)
  {
  },
  
  setJSDefaultStatus : function(status)
  {
  },
  
  setDefaultStatus : function(status)
  {
  },
  
  setOverLink : function(link, b)
  {
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
    
    gBrowser = document.getElementById("content");
    
    gURLBar = document.getElementById("urlbar");
    gURLBar.setAttribute("completedefaultindex", "true");
    
    var currentTab=gBrowser.selectedTab;
    browserInit(currentTab);
    gSelectedTab=currentTab;
    
    gBrowserStatusHandler = new nsBrowserStatusHandler();
    gBrowserStatusHandler.init();
    
    window.XULBrowserWindow = gBrowserStatusHandler;
    window.QueryInterface(nsCI.nsIInterfaceRequestor)
      .getInterface(nsIWebNavigation)
      .QueryInterface(nsCI.nsIDocShellTreeItem).treeOwner
      .QueryInterface(nsCI.nsIInterfaceRequestor)
      .getInterface(nsCI.nsIXULWindow)
      .XULBrowserWindow = window.XULBrowserWindow;
    
    gBrowser.addProgressListener(gBrowserStatusHandler, nsCI.nsIWebProgress.NOTIFY_ALL);
    
    
    window.browserDOMWindow = new nsBrowserAccess(gBrowser);
    
    // Current build was not able to get it. Taking it from the tab browser element. 
    // var webNavigation=gBrowser.webNavigation;
    
    var refBrowser=gBrowser.getBrowserForTab(currentTab);
    var webNavigation=refBrowser.webNavigation;
    
    webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"].createInstance(nsCI.nsISHistory);
    
    gBrowser.docShell.QueryInterface(nsCI.nsIDocShellHistory).useGlobalHistory = true;
    
    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
      .getService(nsCI.nsIBrowserHistory);
    
    gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
      .getService(nsCI.nsIURIFixup);
    
    var bookmarkstore=null; 
    
    try {
      gPref = Components.classes["@mozilla.org/preferences-service;1"]
        .getService(nsCI.nsIPrefBranch);
      
      var page = gPref.getCharPref("browser.startup.homepage");
      var bookmarkstore = gPref.getCharPref("browser.bookmark.store");
      
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
  loadBookmarks(bookmarkstore);
  
  /*
   * Override the title attribute <title /> in this doc with a setter.
   * This is our workaround solution so that when the tabbrowser::updateTitle
   * tries to update this document's title, nothing happens. Bug 311564
   */ 
  
  document.__defineSetter__("title",function(x){}); // Stays with the titled defined by the XUL element. 
  
  
  gBrowser.addEventListener("DOMLinkAdded", BrowserLinkAdded, false);
  
}

/*
 * Page's new Link tag handlers. This should be able to be smart about RSS, CSS, and maybe other Minimo stuff?  
 * So far we have this here, so we can experience and try some new stuff. To be tabrowsed.
 */
function BrowserLinkAdded(event) {
  // ref http://lxr.mozilla.org/mozilla/source/browser/base/content/browser.js#2070
  
  /* 
   * Taken from browser.js - yes this should be in tabbrowser
   */
  
  var erel = event.target.rel;
  var etype = event.target.type;
  var etitle = event.target.title;
  var ehref = event.target.href;
  
  const alternateRelRegex = /(^|\s)alternate($|\s)/i;
  const rssTitleRegex = /(^|\s)rss($|\s)/i;
  
  if (!alternateRelRegex.test(erel) || !etype) return;
  
  etype = etype.replace(/^\s+/, "");
  etype = etype.replace(/\s+$/, "");
  etype = etype.replace(/\s*;.*/, "");
  etype = etype.toLowerCase();
  
  if (etype == "application/rss+xml" || etype == "application/atom+xml" || (etype == "text/xml" || etype == "application/xml" || etype == "application/rdf+xml") && rssTitleRegex.test(etitle))
  {
    
    const targetDoc = event.target.ownerDocument;
    
    var browsers = gBrowser.browsers;
    var shellInfo = null;
    
    for (var i = 0; i < browsers.length; i++) {
      var shell = findChildShell(targetDoc, browsers[i].docShell, null);
      if (shell) shellInfo = { shell: shell, browser: browsers[i] };
    }
    
    //var shellInfo = this._getContentShell(targetDoc);
    
    var browserForLink = shellInfo.browser;
    
    if(!browserForLink) return;
    
    var feeds = [];
    if (browserForLink.feeds != null) feeds = browserForLink.feeds;
    var wrapper = event.target;
    feeds.push({ href: wrapper.href, type: etype, title: wrapper.title});
    browserForLink.feeds = feeds;
    
    if (browserForLink == gBrowser || browserForLink == gBrowser.mCurrentBrowser) {
      var feedButton = document.getElementById("feed-button");
      if (feedButton) {
        feedButton.setAttribute("feeds", "true");
        //				feedButton.setAttribute("tooltiptext", gNavigatorBundle.getString("feedHasFeeds"));	
        document.getElementById("feed-button-menu").setAttribute("oncommand","DoBrowserRSS('"+ehref+"')");
      }
    }
  }
}

function BrowserUpdateFeeds() {
  var feedButton = document.getElementById("feed-button");
  if (!feedButton)
    return;
  
  var feeds = gBrowser.mCurrentBrowser.feeds;
  
  if (!feeds || feeds.length == 0) {
    if (feedButton.hasAttribute("feeds")) feedButton.removeAttribute("feeds");
    //		feedButton.setAttribute("tooltiptext",  gNavigatorBundle.getString("feedNoFeeds"));
  } else {
    feedButton.setAttribute("feeds", "true");
    document.getElementById("feed-button-menu").setAttribute("onpopupshowing","DoBrowserRSS('"+feeds[0].href+"')");
    
    //		feedButton.setAttribute("tooltiptext", gNavigatorBundle.getString("feedHasFeeds"));
  }
}

/* 
 * For now, this updates via DOM the top menu. Context menu should be here as well. 
 */
function BrowserUpdateBackForwardState() {
}


function findChildShell(aDocument, aDocShell, aSoughtURI) {
  aDocShell.QueryInterface(nsCI.nsIWebNavigation);
  aDocShell.QueryInterface(nsCI.nsIInterfaceRequestor);
  var doc = aDocShell.getInterface(nsCI.nsIDOMDocument);
  if ((aDocument && doc == aDocument) || 
      (aSoughtURI && aSoughtURI.spec == aDocShell.currentURI.spec))
    return aDocShell;
  
  var node = aDocShell.QueryInterface(nsCI.nsIDocShellTreeNode);
  for (var i = 0; i < node.childCount; ++i) {
    var docShell = node.getChildAt(i);
    docShell = findChildShell(aDocument, docShell, aSoughtURI);
    if (docShell) return docShell;
  }
  return null;
}


/** 
 * Init stuff
 * 
 **/
function browserInit(refTab)  
{
  /* 
   * addRule access navigational rule to each tab 
   */
  
  refTab.setAttribute("accessrule","focus_content");
  
  /*
   * 
   */
  var refBrowser=gBrowser.getBrowserForTab(refTab);
  
  try {
    refBrowser.markupDocumentViewer.textZoom = .90;
  } catch (e) {
    
  }
  gURLBar = document.getElementById("urlbar");
  
}

function MiniNavShutdown()
{
  if (gBrowserStatusHandler) gBrowserStatusHandler.destroy();
  if(gPrefAdded) {
	try {
      var psvc = Components.classes["@mozilla.org/preferences-service;1"]
        .getService(nsCI.nsIPrefService);
      
      psvc.savePrefFile(null);
      
	} catch (e) { alert(e); }
  }
}

function loadURI(uri)
{
  gBrowser.webNavigation.loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
}

function BrowserBack()
{
  gBrowser.webNavigation.goBack();
}

function BrowserForward()
{
  gBrowser.webNavigation.goForward();
}

function BrowserStop()
{
  gBrowser.webNavigation.stop(nsIWebNavigation.STOP_ALL);
}

function BrowserReload()
{
  gBrowser.webNavigation.reload(nsIWebNavigation.LOAD_FLAGS_NONE);
}

/* 
 * Combine the two following functions in one
 */
function BrowserOpenTab()
{
  try { 
    gBrowser.selectedTab = gBrowser.addTab('about:blank');
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    alert(e);
  }
  //  if (gURLBar) setTimeout(function() { gURLBar.focus(); }, 0);
  
}

/* 
 * Used by the Context Menu - Open link as Tab 
 */
function BrowserOpenLinkAsTab() 
{
  
  if(gFocusedElementHREFContextMenu) {
    try { 
      gBrowser.selectedTab = gBrowser.addTab(gFocusedElementHREFContextMenu);
      browserInit(gBrowser.selectedTab);
    } catch (e) {
      alert(e);
    }
  }
}

/**
 * FOR - keyboard acessibility - context menu for tabbed area *** 
 * Launches the popup for the tabbed area / tabbrowser. Make sure to call this function 
 * when the tabbed panel is available. WARNING somehow need to inform which tab was lack clicked 
 * or mouse over.  
 *
 **/
function BrowserLaunchTabbedPopup() {
  var tabMenu = document.getAnonymousElementByAttribute(document.getElementById("content"),"anonid","tabContextMenu");
  tabMenu.showPopup(gBrowser.selectedTab,-1,-1,"popup","bottomleft", "topleft");
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
 * Deckmode urlbar selector. 
 * Toggles menu item and deckmode.
 */
function BrowserViewDeckSB() {
  BrowserSetDeck(1,document.getElementById("command_ViewDeckSB"));
}

function BrowserViewDeckSearch() {
  BrowserSetDeck(2,document.getElementById("command_ViewDeckSearch"));
}

function BrowserViewDeckDefault() {
  BrowserSetDeck(0,document.getElementById("command_ViewDeckDefault"));
}



/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewSearch() {
  document.getElementById("toolbar-search").collapsed=!document.getElementById("toolbar-search").collapsed;
  if(document.getElementById("toolbar-search").collapsed &&  document.getElementById("command_ViewSearch").getAttribute("checked")=="true") {
	document.getElementById("command_ViewSearch").setAttribute("checked","false");
  }
}


/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewFind() {
  document.getElementById("toolbar-find").collapsed=!document.getElementById("toolbar-find").collapsed;
  if(document.getElementById("toolbar-find").collapsed &&  document.getElementById("command_ViewFind").getAttribute("checked")=="true") {
	document.getElementById("command_ViewFind").setAttribute("checked","false");
  }
}

/** 
 * urlbar indentity, style, progress indicator.
 **/ 
function urlbar() {
}


/* Reset the text size */ 
function BrowserResetZoomPlus() {
  gBrowser.selectedBrowser.markupDocumentViewer.textZoom+= .25;
}

function BrowserResetZoomMinus() {
  gBrowser.selectedBrowser.markupDocumentViewer.textZoom-= .25;
}


/* 
   We want to intercept before it shows, 
   to evaluate when the selected content area is a phone number, 
   thus mutate the popup menu to the right make call item 
*/ 



function BrowserPopupShowing () {
  
  /*
   * Open Link as New Tab  
   */ 
  
  if(document.commandDispatcher.focusedElement && document.commandDispatcher.focusedElement.href) {
	gFocusedElementHREFContextMenu=document.commandDispatcher.focusedElement.href;
	document.getElementById("link_as_new_tab").hidden=false;
    
	document.getElementById("item-backbutton").hidden=true;
	document.getElementById("item-forwardbutton").hidden=true;
	document.getElementById("item-reloadbutton").hidden=true;
    
  } else {
	document.getElementById("link_as_new_tab").hidden=true;
    
	document.getElementById("item-backbutton").hidden=false;
	document.getElementById("item-forwardbutton").hidden=false;
	document.getElementById("item-reloadbutton").hidden=false;
    
  }
  
  var selectedRange=gBrowser.selectedBrowser.contentDocument.getSelection();
  
  /* Enable Copy */
  
  if(selectedRange.toString()) {
    
    document.getElementById("item-copy").style.display="block";
  } else {
    document.getElementById("item-copy").style.display="none";
  }
  
  /* Enable Paste - Can paste only if the focused element has a value attribute. :) 
     THis may affect XHTML nodes. Users may be able to paste things within XML nodes. 
  */
  if (document.commandDispatcher.focusedElement) {
    if(document.commandDispatcher.focusedElement.nodeName=="INPUT"||document.commandDispatcher.focusedElement.nodeName=="TEXTAREA") {
      if(DoClipCheckPaste()) {
        document.getElementById("item-paste").style.display="block";	
      } else {
        document.getElementById("item-paste").style.display="none";	
      }
    }
  }
}


/* Bookmarks */ 

function BrowserBookmarkThis() {
  
  var currentURI=gBrowser.selectedBrowser.webNavigation.currentURI.spec;
  var newLi=gBookmarksDoc.createElement("li");
  var bmContent=gBookmarksDoc.createTextNode(currentURI);
  newLi.appendChild(bmContent);
  gBookmarksDoc.getElementsByTagName("bm")[0].appendChild(newLi);
  gPrefAdded=true;
  
  storeBookmarks();	
  refreshBookmarks();
}

function BrowserBookmark() {
  try {  
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/bookmarks/bmview.xhtml');   
    browserInit(gBrowser.selectedTab);
  } catch (e) {
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserSearch() {
  BrowserViewSearch();
  try { 
    var vQuery=document.getElementById("toolbar-search-tag").value;
    if(vQuery!="") {
      gBrowser.selectedTab = gBrowser.addTab('http://www.google.com/xhtml?q='+vQuery+'&hl=en&lr=&safe=off&btnG=Search&site=search&mrestrict=xhtml');
      browserInit(gBrowser.selectedTab);
    }
  } catch (e) {
    
  }  
}

/* 
 * Search extension to urlbar, deckmode.
 * Called form the deckmode urlbar selector
 */

function DoBrowserSearchURLBAR(vQuery) {
  try { 
    if(vQuery!="") {
      gBrowser.selectedTab = gBrowser.addTab('http://www.google.com/xhtml?q='+vQuery+'&hl=en&lr=&safe=off&btnG=Search&site=search&mrestrict=xhtml');
      browserInit(gBrowser.selectedTab);
    }
  } catch (e) {
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserRSS(sKey) {
  
  if(!sKey) BrowserViewRSS(); // The toolbar is being used. Otherwise it is via the sb: trap protocol. 
  
  try { 
    
    if(sKey) {
      gRSSTag=sKey;
    } else if(document.getElementById("toolbar-rss-rsstag").value!="") {
      gRSSTag=document.getElementById("toolbar-rss-rsstag").value;
    }
    
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/rssview/rssload.xhtml?url='+gRSSTag);
    
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    
  }  
}


/* Toolbar specific code - to be removed from here */ 

function DoBrowserSB(sKey) {
  
  if(!sKey) BrowserViewRSS(); // The toolbar is being used. Otherwise it is via the sb: trap protocol. 
  
  try { 
    if(sKey) {
      gRSSTag=sKey;
    } else if(document.getElementById("toolbar-rss-rsstag").value!="") {
      gRSSTag=document.getElementById("toolbar-rss-rsstag").value;
    }
    
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/rssview/rssload.xhtml?url=http://del.icio.us/rss/tag/'+gRSSTag);
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    
  }  
}

/* Toolbar specific code - to be removed from here */ 


function DoBrowserFind() {
  //  BrowserViewFind();
  try { 
    var vQuery=document.getElementById("toolbar-find-tag").value;
    if(vQuery!="") {
      gBrowser.contentWindow.focus();
      
      /* FIND DOCUMENTATION: 
         41 const FIND_NORMAL = 0;
         42 const FIND_TYPEAHEAD = 1;
         43 const FIND_LINKS = 2;
         http://lxr.mozilla.org/mozilla/source/toolkit/components/typeaheadfind/content/findBar.js
      */
      gBrowser.fastFind.find(vQuery,0);
    }
  } catch (e) {
    alert(e);
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserFindNext() {
  try { 
	gBrowser.fastFind.findNext();
  } catch (e) {
    alert(e);
  }  
}




function DoPanelPreferences() {
  window.openDialog("chrome://minimo/content/preferences/preferences.xul","preferences","modal,centerscreeen,chrome,resizable=no");
  // BrowserReload(); 
}

/* 
   Testing the SMS and Call Services 
*/
function DoTestSendCall(toCall) {
  var phoneInterface= Components.classes["@mozilla.org/phone/support;1"].createInstance(nsCI.nsIPhoneSupport);
  phoneInterface.makeCall(toCall,"");
}

function DoSSRToggle()
{
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(nsCI.nsIPrefBranch);
    pref.setBoolPref("ssr.enabled", !pref.getBoolPref("ssr.enabled"));
    
    gBrowser.webNavigation.reload(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
  }
  catch(ex) { alert(ex); }

}

function DoSNavToggle()
{
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(nsCI.nsIPrefBranch);
    pref.setBoolPref("snav.enabled", !pref.getBoolPref("snav.enabled"));
    
    content.focus();    
  }
  catch(ex) { alert(ex); }

}

function DoFullScreen()
{
  gFullScreen = !gFullScreen;
  
  document.getElementById("nav-bar").hidden = gFullScreen;
  
  // Is this the simpler approach to count tabs? 
  if(gBrowser.mPanelContainer.childNodes.length>1) {
    gBrowser.setStripVisibilityTo(!gFullScreen);
  } 
  
  window.fullScreen = gFullScreen;  
  
  document.getElementById("nav-bar-contextual").hidden = !gFullScreen;    
  
}

/* 
   
 */
function DoClipCopy()
{
  var copytext=gBrowser.selectedBrowser.contentDocument.getSelection().toString();
  var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(nsCI.nsISupportsString);
  if (!str) return false;
  str.data = copytext;
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(nsCI.nsITransferable);
  if (!trans) return false;
  trans.addDataFlavor("text/unicode");
  trans.setTransferData("text/unicode",str,copytext.length * 2);
  var clipid = nsCI.nsIClipboard;
  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].getService(clipid);
  if (!clip) return false;
  clip.setData(trans,null,clipid.kGlobalClipboard);
}

/* 
   Currently supports text/unicode. 
*/
function DoClipCheckPaste()
{
  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].getService(nsCI.nsIClipboard);
  if (!clip) return false;
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(nsCI.nsITransferable);
  if (!trans) return false;
  trans.addDataFlavor("text/unicode");
  clip.getData(trans,clip.kGlobalClipboard);
  var str = new Object();
  var strLength = new Object();
  var pastetext = null;
  trans.getTransferData("text/unicode",str,strLength);
  if (str) str = str.value.QueryInterface(nsCI.nsISupportsString);
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
    if (!gURLBar)
      return;
    
    var url = gURLBar.value;
    if (gURLBar.value == "" || gURLBar.value == null)
      return;
    
    /* Trap to SB 'protocol' */ 
    
    if(gURLBar.value.substring(0,3)=="sb:") {
      DoBrowserSB(gURLBar.value.split("sb:")[1]);
      return;
    }
    
    /* Trap to RSS 'protocol' */ 
    
    if(gURLBar.value.substring(0,4)=="rss:") {
      DoBrowserRSS(gURLBar.value.split("rss:")[1]);
      return;
    }
    
    // SB mode
    if(gDeckMode==1) {
      DoBrowserSB(gURLBar.value);
      BrowserSetDeck(0,document.getElementById("command_ViewDeckDefault"));
      return;
    }
    
    if(gDeckMode==2) {
      DoBrowserSearchURLBAR(gURLBar.value);
      BrowserSetDeck(0,document.getElementById("command_ViewDeckDefault"));
      return;
    }
    /* Other normal cases */ 
    
    var fixedUpURI = gURIFixup.createFixupURI(url, 2 /*fixup url*/ );
    gGlobalHistory.markPageAsTyped(fixedUpURI);
    
    gURLBar.value = fixedUpURI.spec;
    
    loadURI(fixedUpURI.spec);
    
    content.focus();
  }
  catch(ex) {alert(ex);}
  
  
  return true;
}

function PageProxyClickHandler(aEvent) {
  document.getElementById("urlbarModeSelector").showPopup(document.getElementById("proxy-deck"),-1,-1,"popup","bottomleft", "topleft");
}


function URLBarFocusHandler(aEvent, aElt)
{
  
  if (gIgnoreFocus)
    gIgnoreFocus = false;
  else if (gClickSelectsAll)
    aElt.select();
  
  // gURLBar.setAttribute("open", "true"); 
  // gURLBar.showHistoryPopup();
  
  
}

function URLBarMouseDownHandler(aEvent, aElt)
{
  if (aElt.hasAttribute("focused")) { 
    gIgnoreClick = true;
  } else {
    gIgnoreFocus = true;
    gIgnoreClick = false;
    aElt.setSelectionRange(0, 0);
  } 
}

function URLBarClickHandler(aEvent, aElt)
{
  if (!gIgnoreClick && aElt.selectionStart == aElt.selectionEnd)
    aElt.select();
}

/* 
 * Main Menu 
 */ 

function BrowserNavMenuPopup() {
  if (!gShowingNavMenuPopup){
    document.getElementById("menu_NavPopup").showPopup(document.getElementById("nav-menu-button"),-1,-1,"popup","bottomright", "topright");
  }
  else {
    document.getElementById("menu_NavPopup").hidePopup();
  }
  
  gShowingNavMenuPopup != gShowingNavMenuPopup;
}

function BrowserMenuPopup() {
  if(!gShowingMenuPopup) { 
    document.getElementById("menu_MainPopup").showPopup(document.getElementById("menu-button"),-1,-1,"popup","bottomleft", "topleft");
  } else {
    document.getElementById("menu_MainPopup").hidePopup();
  } 

  gShowingMenuPopup != gShowingMenuPopup;
}

function BrowserMenuPopupFalse() {
  document.getElementById("menu_MainPopup").hidePopup();
}


/* The URLBAR Deck mode selector 
 */

function BrowserSetDeck(dMode,menuElement) {
  
  gDeckMode=dMode;
  if(dMode==2) document.getElementById("urlbar-deck").className='search';
  if(dMode==1) document.getElementById("urlbar-deck").className='sb';
  if(dMode==0) document.getElementById("urlbar-deck").className='';
  
}


// ripped from browser.js, this should be shared in toolkit.
function nsBrowserAccess( browser )
{
  this.init(browser);
}

nsBrowserAccess.prototype =
{
  init: function(browser)
  {
    this.browser = browser;
  },
  
  QueryInterface : function(aIID)
  {
    if (aIID.equals(nsCI.nsIBrowserDOMWindow) ||
        aIID.equals(nsCI.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  
  openURI : function(aURI, aOpener, aWhere, aContext)
  {
    var newWindow = null;
    var referrer = null;
    
    var newTab = this.browser.addTab("about:blank");
    
    throw Components.results.NS_NOINTERFACE;
  },
  
  isTabContentWindow : function(aWindow)
  {
    var browsers = gBrowser.browsers;
    for (var ctr = 0; ctr < browsers.length; ctr++)
      if (browsers.item(ctr).contentWindow == aWindow)
        return true;
    return false;
  }
}
  
