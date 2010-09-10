const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const kCountBeforeWeRemember = 5;

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  prompt: function(aRequest) {
    if (aRequest.type != "geolocation")
      return;

    let pm = Services.perms;
    let result = pm.testExactPermission(aRequest.uri, "geo");

    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      aRequest.allow();
      return;
    } else if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      aRequest.cancel();
      return;
    }

    function setPagePermission(aUri, aAllow) {
      let contentPrefs = Services.contentPrefs;

      if (!contentPrefs.hasPref(aRequest.uri, "geo.request.remember"))
        contentPrefs.setPref(aRequest.uri, "geo.request.remember", 0);

      let count = contentPrefs.getPref(aRequest.uri, "geo.request.remember");

      if (aAllow == false)
        count--;
      else
        count++;

      contentPrefs.setPref(aRequest.uri, "geo.request.remember", count);

      if (count == kCountBeforeWeRemember)
        pm.add(aUri, "geo", Ci.nsIPermissionManager.ALLOW_ACTION);
      else if (count == -kCountBeforeWeRemember)
        pm.add(aUri, "geo", Ci.nsIPermissionManager.DENY_ACTION);
    }

    function getChromeWindow(aWindow) {
      let chromeWin = aWindow
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShellTreeItem)
        .rootTreeItem
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow)
        .QueryInterface(Ci.nsIDOMChromeWindow);
      return chromeWin;
    }

    let notificationBox = null;
    if (aRequest.window) {
      let requestingWindow = aRequest.window.top;
      let chromeWin = getChromeWindow(requestingWindow).wrappedJSObject;
      notificationBox = chromeWin.getNotificationBox(requestingWindow);
    } else {
      let chromeWin = aRequest.element.ownerDocument.defaultView;
      notificationBox = chromeWin.Browser.getNotificationBox();
    }

    let notification = notificationBox.getNotificationWithValue("geolocation");
    if (!notification) {
      let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

      let buttons = [{
        label: browserBundle.GetStringFromName("geolocation.share"),
        accessKey: null,
        callback: function(notification) {
          setPagePermission(aRequest.uri, true);
          aRequest.allow();
        },
      },
      {
        label: browserBundle.GetStringFromName("geolocation.dontShare"),
        accessKey: null,
        callback: function(notification) {
          setPagePermission(aRequest.uri, false);
          aRequest.cancel();
        },
      }];

      let message = browserBundle.formatStringFromName("geolocation.siteWantsToKnow",
                                                       [aRequest.uri.host], 1);

      let newBar = notificationBox.appendNotification(message,
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
const NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);
