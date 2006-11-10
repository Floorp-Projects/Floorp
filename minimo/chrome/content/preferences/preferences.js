/*
 * Utils 
 */ 

function fixXULAutoScroll(e) {

      var referenceScrollerBoxObject = document.getElementById("scroller").boxObject;
      var referenceBottomBoxObject = document.getElementById("refBottom").boxObject;
      var refTopY = referenceScrollerBoxObject.y;

	try {
		scb = referenceScrollerBoxObject.QueryInterface(Components.interfaces.nsIScrollBoxObject);
		try {	
			focusedElementBoxObject = document.getBoxObjectFor(e.target);
			var vx={}; var vy={};
			var currentPosY = scb.getPosition(vx,vy);
			var toPos = parseInt(focusedElementBoxObject.y-refTopY);

			if(focusedElementBoxObject) {
				if(parseInt(focusedElementBoxObject.y-(vy.value))>referenceBottomBoxObject.y) {
					scb.scrollToElement(e.target);

					scb.scrollTo(0,parseInt(focusedElementBoxObject.y-referenceBottomBoxObject.y+focusedElementBoxObject.height+2));
				}
			}

		} catch (i) {
		}

	} catch (i) {
	}
}


/* 
 * Translators - When Read, When Write
 * =====
 * These are usually simple DOM XML translators. We need to make this XBL-based elements.
 */ 


function readEnableImagesPref()
{
  // get the pref value as it is (String, int or bool).
  var pref = document.getElementById("permissions.default.image");
  return (pref.value == 1);
}

function writeEnableImagesPref()
{ 
  var checkbox = document.getElementById("enableImages");
  if (checkbox.checked==true) {
    return 1;
  } else {
    return 2;
  } 
}
  
function readProxyPref()
{
  // get the pref value as it is (String, int or bool).
  var pref = document.getElementById("network.proxy.type");
  return (pref.value == 1);
}

function writeProxyPref()
{ 
  var checkbox = document.getElementById("UseProxy");
  if (checkbox.checked) {
    return 1;
  }
  return 0;
}

function readCacheLocationPref()
{ 
  // get the pref value as it is (String, int or bool).
  var pref = document.getElementById("browser.cache.disk.parent_directory");
  if (pref.value)
      return true;
  else
      return false;
}

function writeCacheLocationPref()
{ 
  // set the visual element. 
  var checkbox = document.getElementById("storeCacheStorageCard");
  if (checkbox.checked==true) {
    return "\\Storage Card\\Minimo Cache";
  } else {
    return "";
  } 
}


/* 
 * UI Visual elements disabled, enabled, dependency.
 */
 
function UIdependencyCheck() {

  if(!document.getElementById("useDiskCache").value) {
	//document.getElementById("storeCacheStorageCard").disabled=true;
	document.getElementById("cacheSizeField").disabled=true;
  } else {
	//document.getElementById("storeCacheStorageCard").disabled=false;
	document.getElementById("cacheSizeField").disabled=false;
  }

  if(!document.getElementById("UseProxy").checked) {
	document.getElementById("networkProxyHTTP").disabled=true;
	document.getElementById("networkProxyHTTP_Port").disabled=true;
  } else {
	document.getElementById("networkProxyHTTP").disabled=false;
	document.getElementById("networkProxyHTTP_Port").disabled=false;
  }

}

/*
 * OnReadPref callbacks 
 */

/*
 * Depends on dontAskForLaunch pref available. Check Preferences.xul order. 
 */
 
function downloadSetTextbox()
{

  var dirLocation=document.getElementById("downloadDir").value;
  
  try {
   const nsIDirectoryServiceProvider2 = Components.interfaces.nsIDirectoryServiceProvider2;
   const nsIDirectoryServiceProvider_CONTRACTID = "@mozilla.org/device/directory-provider;1";
   var dirServiceProvider = Components.classes[nsIDirectoryServiceProvider_CONTRACTID].getService(nsIDirectoryServiceProvider2);
   var files = dirServiceProvider.getFiles("SCDirList");

   if ( files.hasMoreElements() ) {
	document.getElementById("menuDownloadOptions").setAttribute("hidden","false");
   }
   
   var fileId=0;

   while (files.hasMoreElements())
   {
     var file = files.getNext();
     const nsILocalFile = Components.interfaces.nsILocalFile;
 	 var file2 = file.QueryInterface(nsILocalFile);
	 fileId++;	
     var newElement=document.createElement("menuitem");

	 newElement.setAttribute("label",file2.path);
	 
	 newElement.setAttribute("id","fileDownloadOption"+fileId);
	 newElement.fileValue=file2;
	 newElement.setAttribute("oncommand", "downloadSelectedOption('fileDownloadOption"+fileId+"')");
	 document.getElementById("downloadOptionsList").appendChild(newElement);

     if(dirLocation.path==file2.path) { 
		if(document.getElementById("dontAskForLaunch").value) {
		       document.getElementById("menuDownloadOptions").selectedItem=newElement;
		} else {
		       document.getElementById("menuDownloadOptions").selectedItem=document.getElementById("downloadPromptOption");
		}
     }
   }
 } catch (i) { } 

}

function downloadSelectedOption(refId)
{

  var refElementSelected = document.getElementById(refId);
  document.getElementById("downloadDir").value=refElementSelected.fileValue;
  syncPref(document.getElementById("downloadDir"));

  document.getElementById("dontAskForLaunch").value=true;
  syncPref(document.getElementById("dontAskForLaunch"));

}

function downloadSelectPrompt() {

  document.getElementById("dontAskForLaunch").value=false;
  syncPref(document.getElementById("dontAskForLaunch"));

}

/*
 * Depends on usaDiskCache pref enabled. Check Preferences.xul order. 
 */
function cacheSetTextbox() {

  var dirLocation=document.getElementById("storeCacheStorageCard").value;
  
  try {
   const nsIDirectoryServiceProvider2 = Components.interfaces.nsIDirectoryServiceProvider2;
   const nsIDirectoryServiceProvider_CONTRACTID = "@mozilla.org/device/directory-provider;1";
   var dirServiceProvider = Components.classes[nsIDirectoryServiceProvider_CONTRACTID].getService(nsIDirectoryServiceProvider2);
   var files = dirServiceProvider.getFiles("SCDirList");

   if( files.hasMoreElements() ) {
	document.getElementById("menuCacheOptions").setAttribute("hidden","false");
   }
   
   var fileId=0;

   while (files.hasMoreElements())
   {
     var file = files.getNext();
     const nsILocalFile = Components.interfaces.nsILocalFile;
 	 var file2 = file.QueryInterface(nsILocalFile);
	 fileId++;	
       var newElement=document.createElement("menuitem");
	 newElement.setAttribute("label",file2.path);
	 newElement.setAttribute("id","fileCacheOption"+fileId);
	 newElement.fileValue=file2;
	 newElement.setAttribute("oncommand", "cacheSelectedOption('fileCacheOption"+fileId+"')"     );
	 document.getElementById("cacheOptionsList").appendChild(newElement);

     if(dirLocation.path==file2.path) { 
		if(document.getElementById("useDiskCache").value) {

		       document.getElementById("menuCacheOptions").selectedItem=newElement;
		} else {
		       document.getElementById("menuCacheOptions").selectedItem=document.getElementById("cacheNoneOption");
		}
     }
   }
 } catch (i) { } 
}

function cacheSelectedOption(refId)
{

  var refElementSelected = document.getElementById(refId);
  document.getElementById("storeCacheStorageCard").value=refElementSelected.fileValue;
  syncPref(document.getElementById("storeCacheStorageCard"));

  document.getElementById("useDiskCache").value=true;
  syncPref(document.getElementById("useDiskCache"));

}

function cacheSelectNone() {

  document.getElementById("useDiskCache").value=false;
  syncPref(document.getElementById("useDiskCache"));

}

/*
 * Synchronous pref actions
 */

function sanitizeAll()
{

    // GPS Permissions
    try
    {
        var permMgr = Components.classes["@mozilla.org/permissionmanager;1"]
                                .getService(Components.interfaces.nsIPermissionManager);

        var enumerator = permMgr.enumerator;
        while (enumerator.hasMoreElements()) {
             var nextPermission = enumerator.getNext();
             nextPermission = nextPermission.QueryInterface(Components.interfaces.nsIPermission); 
             if (nextPermission.type == "GPS") {
                permMgr.remove(nextPermission.host, "GPS");   
            }
        }

    } catch (e) { alert(e) }

    // Cookies
    try 
    {
        var cookieMgr = Components.classes["@mozilla.org/cookiemanager;1"]
                                  .getService(Components.interfaces.nsICookieManager);
        cookieMgr.removeAll()
    } catch (e) { }    


    // Form Data
    try 
    {
        var formHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                                    .getService(Components.interfaces.nsIFormHistory);
        formHistory.removeAllEntries();
    } catch (e) { }

    // Cache 
    try 
    {
        var classID = Components.classes["@mozilla.org/network/cache-service;1"];
        var cacheService = classID.getService(Components.interfaces.nsICacheService);
        cacheService.evictEntries(Components.interfaces.nsICache.STORE_IN_MEMORY);
        cacheService.evictEntries(Components.interfaces.nsICache.STORE_ON_DISK);
    } catch (e) { }
    
    // Autocomplete
    try 
    {
        var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                      .getService(Components.interfaces.nsIBrowserHistory);
        globalHistory.removeAllPages();
    } catch (e) { }
         
    // Session History
    try 
    {
       var os = Components.classes["@mozilla.org/observer-service;1"]
                          .getService(Components.interfaces.nsIObserverService);
        os.notifyObservers(null, "browser:purge-session-history", "");
    } catch (e) { }    
    document.getElementById("privacySanitize").disabled=true;
}

function sanitizeBookmarks() {
	// in Common.
	BookmarksDeleteAllAndSync();
    document.getElementById("bookmarksSanitize").disabled=true;
}

function loadHomePageFromBrowser() {
 var win;
 var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
 win = wm.getMostRecentWindow("navigator:browser");
 if(!win) win = window.opener;
 if (win) {
   var homePageField = document.getElementById("browserStartupHomepage");
   var newVal = "";
   var tabbrowser = win.document.getElementById("content");
   var l = tabbrowser.browsers.length;

   var firstAdded = false;

   for (var i = 0; i < l; i++) {
      var url = tabbrowser.getBrowserAtIndex(i).webNavigation.currentURI.spec;
      
      if (url == "chrome://minimo/content/preferences/preferences.xul")
          continue;

      if (firstAdded)
         newVal += "|";
      else
          firstAdded = true;

      newVal += url;
   }
   homePageField.value = newVal;
   syncPref(homePageField);

  }
}

function loadHomePageBlank() {
   var homePageField = document.getElementById("browserStartupHomepage");
   homePageField.value = "about:blank";
   syncPref(homePageField);
}

function setDefaultBrowser() {

  /* In the device, there is a live synch here. With desktop we fail nice here, so 
     the pref can be set */

  try { 

    var device = Components.classes["@mozilla.org/device/support;1"].getService(nsIDeviceSupport);

    if(document.getElementById("setDefaultBrowser").checked) {

      if( !device.isDefaultBrowser() )
         device.setDefaultBrowser();

    }

  } catch (e) {

  }

}

/*
 * Rewrite of Prefs for Minimo.
 * ----------------------------
 * Quick access to OKAY / CANCEL button 
 * Panel selector 
 * OKAY = sync attributes with prefs. 
 * CANCEL = close popup 
 */

var gPrefQueue=new Array();
var gCancelSync=false;
var gPaneSelected=null;
var gToolbarButtonSelected=null;
var gPrefArray=null;
var gPref=null;

function prefStartup() {

    /* fix the size of the scrollbox contents */

	//marcio 4000
 
    innerWidth = document.getBoxObjectFor(document.getElementById("scroller")).width;
    document.getElementById("pref-panes").style.width=innerWidth+"px";

    /* Pre select the general pane */ 

    gPanelSelected=document.getElementById("general-pane");
    gToolbarButtonSelected=document.getElementById("general-button");
    gToolbarButtonSelected.className="base-button prefselectedbutton";  // local to preferences.css (may have to be promoted minimo.css)

    /* Initialize the pref service instance */ 

    try {
      gPref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);

    } catch (ignore) { alert(ignore)}

    syncPrefLoadDOM(document.getElementById("prefsInstance").prefArray);

    gToolbarButtonSelected.focus();
}

/*
 * This is called with onclose onunload event
 * This may be canceled if user clicks cancel 
 */
function prefShutdown() {
      if(!gCancelSync) {
		syncPrefSaveDOM();
	}
}

/* 
 * Called from the XUL 
 * --
 * When user dispatches oncommand for a given toolbarbutton on top
 * we dispatch a focus to an item. 
 */ 

function focusSkipToPanel() {
	
	try {

		document.commandDispatcher.advanceFocusIntoSubtree(document.getElementById("pref-panes"));

	} catch (e) { alert(e) }

}


/* 
 * Called from the XUL 
 * -- 
 * When user moves focus over a toolbarbutton item on top
 * we should show the right pane. 
 */

function show(idPane,toolbarButtonRef) {

    if(gPanelSelected) {
        gPanelSelected.collapsed="true";
        gToolbarButtonSelected.className="base-button";
    } 
    gPanelSelected=document.getElementById(idPane);
    gToolbarButtonSelected=toolbarButtonRef;
    gToolbarButtonSelected.className="base-button prefselectedbutton";  // local to preferences.css
    gPanelSelected.collapsed=false;
}

/* 
 * Each changed to some prefs, may cal this syncPref
 * which stores in a Queue global. 
 * The OKAY sync function should go through each, 
 * and sync with pref service. 
 */

function syncPref(refElement) {
	var refElementPref=refElement.getAttribute("preference");
	if(refElementPref!="") {
		gPrefQueue[refElementPref]=refElement;
		//document.getElementById("textbox-okay-pane").value+= "Changed key ="+gPrefQueue[refElementPref].value+"\n";
	}
	setTimeout("UIdependencyCheck()",0);

	document.getElementById("okay-button").setAttribute("disabled",false);
	
}


/*
 * Okay, just close the window with sync mode. 
 */

function PrefOkay() {

	document.getElementById("okay-button").setAttribute("disabled",true);

}

/* 
 * Cancel, close window but do not sync. 
 */

function PrefCancel() {
    gCancelSync=true;
}


function syncPrefSaveDOM() {
	try {
		var psvc = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefService);

		for(var strCurKey in gPrefQueue) {
			var elRef=gPrefQueue[strCurKey];
			var prefName=elRef.getAttribute("preference");
			var transValidator=elRef.getAttribute("onsynctopreference");
			var prefSETValue=null;

			if(transValidator!="") {
				prefSETValue=eval(transValidator);
	
			} else {
				if(elRef.nodeName=="checkbox") {
			            if(elRef.checked) {
			                elRef.value=true;
			            } else {
			                elRef.value=false;
			            } 
				}
				prefSETValue=elRef.value;

			}

			if (document.getElementById(prefName).getAttribute("preftype")=="string"){
				try { 
					gPref.setCharPref(prefName, prefSETValue);
				} catch (e) { } 
			} 
	
			if (document.getElementById(prefName).getAttribute("preftype")=="int") {
				try { 
				gPref.setIntPref(prefName, prefSETValue);
				} catch (e) { } 
	 	 	}
	
			if (document.getElementById(prefName).getAttribute("preftype")=="bool") {
				try { 
				gPref.setBoolPref(prefName, prefSETValue);
				} catch (e) { } 
			}

                  if (document.getElementById(prefName).getAttribute("preftype")=="file") {

                   var lf;
	             
                   //if (typeof(val) == "string") {
                   //   lf = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
                   //   lf.persistentDescriptor = val;
                   //   if (!lf.exists())
                   //      lf.initWithPath(val);
                   // }
	             //else lf = prefSETValue.QueryInterface(Components.interfaces.nsILocalFile);

	             lf = prefSETValue.QueryInterface(Components.interfaces.nsILocalFile);
                   gPref.setComplexValue(prefName, Components.interfaces.nsILocalFile, lf);
	 
                   }	

		}

		psvc.savePrefFile(null);

	} catch (e) { alert(e); }

}
function syncPrefLoadDOM(elementList) {

	for(var strCurKey in elementList) {

		var elementAndPref=document.getElementById(elementList[strCurKey]);
		var prefName=elementAndPref.getAttribute("preference");
		var prefUIType=elementAndPref.getAttribute("prefuitype");
		var transValidator=elementAndPref.getAttribute("onsyncfrompreference");
		var onReadPref=elementAndPref.getAttribute("onreadpref");

		var prefDOMValue=null;

		if (document.getElementById(prefName).getAttribute("preftype")=="file") {

		    try {

		            prefDOMValue = gPref.getComplexValue(prefName, Components.interfaces.nsILocalFile);

                } catch (ex) { prefDOMValue=null; } 

		    document.getElementById(prefName).value=prefDOMValue;

			if(transValidator) {
				preGETValue=eval(transValidator);
				if(prefUIType=="string" || prefUIType=="int") elementAndPref.value=preGETValue;
				if(prefUIType=="bool") elementAndPref.checked=preGETValue;
			} else {
				elementAndPref.value=prefDOMValue;
			}		
		    

			if(onReadPref) {
				eval(onReadPref);
			}

		} 


		if (document.getElementById(prefName).getAttribute("preftype")=="string") {

		    try {
		 	   prefDOMValue = gPref.getCharPref(prefName);
                } catch (ex) { prefDOMValue=null; } 

		    document.getElementById(prefName).value=prefDOMValue;

			if(transValidator) {
				preGETValue=eval(transValidator);
				if(prefUIType=="string" || prefUIType=="int") elementAndPref.value=preGETValue;
				if(prefUIType=="bool") elementAndPref.checked=preGETValue;
			} else {
				elementAndPref.value=prefDOMValue;
			}		
		    
		} 

		if (document.getElementById(prefName).getAttribute("preftype")=="int") {

		    try {
			    prefDOMValue = gPref.getIntPref(prefName);
                } catch (ex) { prefDOMValue=null; } 
		    document.getElementById(prefName).value=prefDOMValue;

			if(transValidator) {
				preGETValue=eval(transValidator);
				if(prefUIType=="string" || prefUIType=="int") elementAndPref.value=preGETValue;
				if(prefUIType=="bool") elementAndPref.checked=preGETValue;
			} else {
				if(prefDOMValue==1) { 
				   elementAndPref.checked=true;
				} else {
				   elementAndPref.checked=false;
				} 
				elementAndPref.value=prefDOMValue ;
			}
 	 	}

		if (document.getElementById(prefName).getAttribute("preftype")=="bool") {

		    try { 
		 	   prefDOMValue = gPref.getBoolPref(prefName);
                } catch (ex) { prefDOMValue=null; } 

			    document.getElementById(prefName).value=prefDOMValue;

			if(transValidator) {
				preGETValue=eval(transValidator);
				if(prefUIType=="string" || prefUIType=="int") elementAndPref.value=preGETValue;
				if(prefUIType=="bool") elementAndPref.checked=preGETValue;
			} else {
				if(prefDOMValue==true) { 
				   elementAndPref.checked=true;
				} else {
				   elementAndPref.checked=false;
				} 
				elementAndPref.value=prefDOMValue;
			}	
		}

		
	}
	UIdependencyCheck();
}



function prefFocus(el) {
	document.getElementById(el).className="box-prefgroupitem2";
}

function prefBlur(el) {
	document.getElementById(el).className="box-prefgroupitem";
}
