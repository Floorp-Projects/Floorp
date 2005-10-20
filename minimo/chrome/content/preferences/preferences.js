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
  if (checkbox.checked) {
    return 1;
  }
  return 2;
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

    /* This should be moved to the XUL, the list of pref items.  */ 

    var regPrefElements= new Array ( 'browserStartupHomepage', 'enableImages', 'ssr', 'sitessr','skey', 'dumpJS', 'UseProxy', 'networkProxyHTTP', 'networkProxyHTTP_Port', 'browserDisplayZoomUI', 'browserDisplayZoomContent'  );

    /* Initialize the pref service instance */ 

    try {
      gPref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);

    } catch (ignore) { alert(ignore)}
    syncPrefLoadDOM(regPrefElements);

    syncUIZoom(); // from common.js 

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

var gPrefQueue=new Array();

function syncPref(refElement) {

	var refElementPref=refElement.getAttribute("preference");
	if(refElementPref!="") {
		// has pref sync definition,..

		/* Assume it's a textbox */

		gPrefQueue[refElementPref]=refElement;

            document.getElementById("textbox-okay-pane").value+= "Changed key ="+gPrefQueue[refElementPref].value+"\n";
				
	}
}


/*
 * OKAY Sync with prefService 
 */

function PrefOkay() {



}

/* 
 * Cancel - nothing to do, close window 
 */

function PrefCancel() {
	window.close();
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





