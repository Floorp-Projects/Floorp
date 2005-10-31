/* 
 * Translators function. 
 * In XUL and beyong, these mappers shall happen more and more 
 * as we get more hybrid data-types. It's just like Webservice proxies for XUL elemetns. 
 *. So far we have this static hardcoded her
 * ===================================================================================
 */ 


function readEnableImagesPref()
{
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


/* Live Synchronizers, 
 * In this section put all the functions you think it should be 
 * synchronized as the end-user hits the button 
 * ===================================================================================
 */

function sanitizeAll()
{
    // Cache 
    var classID = Components.classes["@mozilla.org/network/cache-service;1"];
    var cacheService = classID.getService(Components.interfaces.nsICacheService);
    cacheService.evictEntries(Components.interfaces.nsICache.STORE_IN_MEMORY);
    cacheService.evictEntries(Components.interfaces.nsICache.STORE_ON_DISK);
    
    // Autocomplete
    var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                  .getService(Components.interfaces.nsIBrowserHistory);
    globalHistory.removeAllPages();
         
    try 
    {
       var os = Components.classes["@mozilla.org/observer-service;1"]
                          .getService(Components.interfaces.nsIObserverService);
        os.notifyObservers(null, "browser:purge-session-history", "");
    } catch (e) { }    

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
            document.getElementById("textbox-okay-pane").value+= "Changed key ="+gPrefQueue[refElementPref].value+"\n";
	}
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
		if (gPref.getPrefType(prefName) == gPref.PREF_STRING){
			gPref.setCharPref(prefName, prefSETValue);
		} 

		if (gPref.getPrefType(prefName) == gPref.PREF_INT) {
			gPref.setIntPref(prefName, prefSETValue);
 	 	}

		if (gPref.getPrefType(prefName) == gPref.PREF_BOOL) {
			gPref.setBoolPref(prefName, prefSETValue);
		}
	}
}

function syncPrefLoadDOM(elementList) {
	for(var strCurKey in elementList) {

		var elementAndPref=document.getElementById(elementList[strCurKey]);
		var prefName=elementAndPref.getAttribute("preference");
		var prefType=elementAndPref.getAttribute("preftype");

		var prefDOMValue=null;
		if (gPref.getPrefType(prefName) == gPref.PREF_STRING){
		    prefDOMValue = gPref.getCharPref(prefName);
		} 

		if (gPref.getPrefType(prefName) == gPref.PREF_INT) {

		    prefDOMValue = gPref.getIntPref(prefName);

			if(prefDOMValue==1) { 
			   elementAndPref.checked=true;
			} else {
			   elementAndPref.checked=false;
			} 
 	 	}

		if (gPref.getPrefType(prefName) == gPref.PREF_BOOL) {
		    prefDOMValue = gPref.getBoolPref(prefName);
			if(prefDOMValue==true) { 
			   elementAndPref.checked=true;
			} else {
			   elementAndPref.checked=false;
			} 
		}

		elementAndPref.value=prefDOMValue;

	}
}


function prefFocus(el) {
	document.getElementById(el).className="box-prefgroupitem2";
}

function prefBlur(el) {
	document.getElementById(el).className="box-prefgroupitem";
}
