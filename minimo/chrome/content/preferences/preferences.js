/* 
 * Translators function. 
 * =====
 * With XUL and beyond, these mappers shall happen more and more 
 * as we get more hybrid data-types. It's just like Webservice proxies for XUL elemetns. 
 *. So far we have this static hardcoded here. These has to do with the onsyncfrompreference
 * and onsynctopreference attributes in the XUL pref panels. 
 * ===================================================================================
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
 * This is called after pref -> DOM load. 
 * and also when clicks sync happens - see each pref element item the onchange attribute
 */
 
function UIdependencyCheck() {
  if(!document.getElementById("useDiskCache").checked) {
	document.getElementById("storeCacheStorageCard").disabled=true;
	document.getElementById("cacheSizeField").disabled=true;
  } else {
	document.getElementById("storeCacheStorageCard").disabled=false;
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



/* Live Synchronizers
 * =====
 * In this section put all the functions you think it should be 
 * synchronized as the end-user hits the button 
 * ===================================================================================
 */

function sanitizeAll()
{

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


/*
 * New Mini Pref Implementation 
 * ----------------------------
 * Quick access to OKAY / CANCEL button 
 * Panel selector 
 * OKAY = sync attributes with prefs. 
 * CANCEL = close popup 
 * ===================================================================================
 */

var gPrefQueue=new Array();
var gCancelSync=false;
var gPaneSelected=null;
var gToolbarButtonSelected=null;
var gPrefArray=null;
var gPref=null;

function prefStartup() {
    keyInitHandler();

    /* Pre select the general pane */ 

    gPanelSelected=document.getElementById("general-pane");
    gToolbarButtonSelected=document.getElementById("general-button");
    gToolbarButtonSelected.className="base-button prefselectedbutton";  // local to preferences.css (may have to be promoted minimo.css)

    /* Initialize the pref service instance */ 

    try {
      gPref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);

    } catch (ignore) { alert(ignore)}

    // This is now deprecated, since we have XBL-defined elements to handle the list of registered pref ID elements. 
    // var regPrefElements= new Array ( 'browserStartupHomepage', 'enableImages', 'ssr', 'sitessr','skey', 'dumpJS', 'UseProxy', 'networkProxyHTTP', 'networkProxyHTTP_Port', 'browserDisplayZoomUI', 'browserDisplayZoomContent'  );
    // Now depends on the preferenceset.preferenceitem XBL implementation. 
    // in minimo/content/bindings/preferences.css and preferences.xml

    syncPrefLoadDOM(document.getElementById("prefsInstance").prefArray);

    syncUIZoom(); // from common.js 

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

/* General Access keyboard handler */ 

function keyInitHandler() {

  document.addEventListener("keypress",eventHandlerMenu,true);
  
}

/* Called directly from the XUL top toolbar items */ 

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
 * Followin two functions inherited (yes copy and paste) from the MInimo.js. Has 
 * Some differences, shall be combined in one or completelly elimiated when XBL 
 * elements ready 
 */ 

function eventHandlerMenu(e) {

  if(e.charCode==109) {
  	document.getElementById("general-button").focus();
    e.preventBubble();
  } 
 
   if(e.keyCode==134 || e.keyCode==70) /*SoftKey1 or HWKey1*/ {
  	document.getElementById("general-button").focus();
    e.preventBubble();
  } 

  var outnavTarget=document.commandDispatcher.focusedElement.getAttribute("accessrule");

  if(!outnavTarget && (e.keyCode==40||e.keyCode==38)) {
    e.preventDefault();
	if(e.keyCode==38) { 
		document.commandDispatcher.rewindFocus();
	}
	if(e.keyCode==40) {
		document.commandDispatcher.advanceFocus();
	}

  }
  if(outnavTarget!="" && (e.keyCode==40||e.keyCode==38)) {
      e.preventBubble();
      if(e.keyCode==40) {
        ruleElement=findRuleById(document.getElementById(outnavTarget).getAttribute("accessnextrule"),"accessnextrule");
      }
      if(e.keyCode==38) {
        ruleElement=findRuleById(document.getElementById(outnavTarget).getAttribute("accessprevrule"),"accessprevrule"); 
      }
	  var tempElement=ruleElement.getAttribute("accessfocus");
      if(tempElement.indexOf("#")>-1) {
      
        if(tempElement=="#tabContainer") { 
          if(ruleElement.tabContainer) {
            ruleElement.selectedTab.focus();
          }
        } 
		if(tempElement=="#tabContent") { 
          document.commandDispatcher.advanceFocusIntoSubtree(document.getElementById("nav-bar"));
          document.commandDispatcher.rewindFocus();
        } 
        
	  } else { 
		  document.getElementById(tempElement).focus();
	  }
  }
}

function findRuleById(outnavTarget,ruleattribute) {
  var ruleElement=document.getElementById(outnavTarget);

  if(ruleElement.collapsed) {
      return findRuleById(ruleElement.getAttribute(ruleattribute), ruleattribute);
  } else {
	return ruleElement;
  } 
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
}


/*
 * Okay, just close the window with sync mode. 
 */

function PrefOkay() {
	window.close();
}

/* 
 * Cancel, close window but do not sync. 
 */

function PrefCancel() {
    gCancelSync=true;
	window.close();
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

		var prefDOMValue=null;

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
