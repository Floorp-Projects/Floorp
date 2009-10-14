const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const kCountBeforeWeRemember = 5;

function GeolocationPrompt() {}

GeolocationPrompt.prototype = {
  classDescription: "Geolocation Prompting Component",
  classID:          Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),
  contractID:       "@mozilla.org/geolocation/prompt;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGeolocationPrompt]),
 
  prompt: function(request) {
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
    var result = pm.testExactPermission(request.requestingURI, "geo");
    
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return;
    }
    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return;
    }

    function setPagePermission(uri, allow) {
      var prefService = Cc["@mozilla.org/content-pref/service;1"].getService(Ci.nsIContentPrefService);
        
      if (! prefService.hasPref(request.requestingURI, "geo.request.remember"))
        prefService.setPref(request.requestingURI, "geo.request.remember", 0);
      
      var count = prefService.getPref(request.requestingURI, "geo.request.remember");
      
      if (allow == false)
        count--;
      else
        count++;

      prefService.setPref(request.requestingURI, "geo.request.remember", count);
        
      if (count == kCountBeforeWeRemember)
        pm.add(uri, "geo", Ci.nsIPermissionManager.ALLOW_ACTION);
      else if (count == -kCountBeforeWeRemember)
        pm.add(uri, "geo", Ci.nsIPermissionManager.DENY_ACTION);
    }

    function getChromeWindow(aWindow) {
      var chromeWin = aWindow 
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShellTreeItem)
        .rootTreeItem
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow)
        .QueryInterface(Ci.nsIDOMChromeWindow);
      return chromeWin;
    }

    var requestingWindow = request.requestingWindow.top;
    var chromeWin = getChromeWindow(requestingWindow).wrappedJSObject;

    var notificationBox = chromeWin.getNotificationBox(requestingWindow);

    var notification = notificationBox.getNotificationWithValue("geolocation");
    if (!notification) {
      var bundleService = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
      var browserBundle = bundleService.createBundle("chrome://browser/locale/browser.properties");

      var buttons = [{
        label: browserBundle.GetStringFromName("geolocation.share"),
        accessKey: null,
        callback: function(notification) {
          setPagePermission(request.requestingURI, true);
          request.allow(); 
        },
      },
      {
        label: browserBundle.GetStringFromName("geolocation.dontShare"),
        accessKey: null,
        callback: function(notification) {
          setPagePermission(request.requestingURI, false);
          request.cancel();
        },
      }];
      
      var message = browserBundle.formatStringFromName("geolocation.siteWantsToKnow",
                                                       [request.requestingURI.host], 1);      
      
      var newBar = notificationBox.appendNotification(message,
                                                      "geolocation",
                                                      "chrome://browser/skin/images/geo-16.png",
                                                      notificationBox.PRIORITY_WARNING_MEDIUM,
                                                      buttons);
      // Make this a geolocation notification.
      newBar.setAttribute("type", "geo"); 
    }
  }
};


//module initialization
function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([GeolocationPrompt]);
}
