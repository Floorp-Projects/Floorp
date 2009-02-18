const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function GeolocationPrompt() {}

GeolocationPrompt.prototype = {
  classDescription: "Geolocation Prompting Component",
  classID:          Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),
  contractID:       "@mozilla.org/geolocation/prompt;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGeolocationPrompt]),
 
  prompt: function(request) {

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
        label: browserBundle.GetStringFromName("geolocation.exactLocation"),
        accessKey: browserBundle.GetStringFromName("geolocation.exactLocationKey"),
        callback: function() request.allow()
        },
        {
        label: browserBundle.GetStringFromName("geolocation.nothingLocation"),
        accessKey: browserBundle.GetStringFromName("geolocation.nothingLocationKey"),
        callback: function() request.cancel()
        }];
      
      var message = browserBundle.formatStringFromName("geolocation.requestMessage",
                                                       [request.requestingURI.spec], 1);      
      notificationBox.appendNotification(message,
                                         "geolocation",
                                         "chrome://browser/skin/Info.png",
                                         notificationBox.PRIORITY_INFO_HIGH,
                                         buttons);
    }
  }
};


//module initialization
function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([GeolocationPrompt]);
}
