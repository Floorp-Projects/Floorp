
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

function loadIntoWindow(window) {}
function unloadFromWindow(window) {}

function _sendMessageToJava (aMsg) {
  return Services.androidBridge.handleGeckoMessage(aMsg);
};

/*
 bootstrap.js API
*/
var windowListener = {
  onOpenWindow: function(aWindow) {
    // Wait for the window to finish loading
    let domWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    domWindow.addEventListener("load", function() {
      domWindow.removeEventListener("load", arguments.callee, false);
      if (domWindow) {
        domWindow.addEventListener("scroll", function(e) {
          let message = {
            type: 'robocop:scroll',
            y: XPCNativeWrapper.unwrap(e.target).documentElement.scrollTop,
            height: XPCNativeWrapper.unwrap(e.target).documentElement.scrollHeight,
            cheight: XPCNativeWrapper.unwrap(e.target).documentElement.clientHeight,
          };
          _sendMessageToJava(message);
        });
      }
    }, false);
  },
  onCloseWindow: function(aWindow) { },
  onWindowTitleChange: function(aWindow, aTitle) { }
};

function startup(aData, aReason) {
  let wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

  // Load into any new windows
  wm.addListener(windowListener);
  Services.obs.addObserver(function observe(aSubject, aTopic, aData) {
      dump("Robocop:Quit received -- requesting quit");
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eForceQuit);
  }, "Robocop:Quit", false);
}

function shutdown(aData, aReason) {
  // When the application is shutting down we normally don't have to clean up any UI changes
  if (aReason == APP_SHUTDOWN) return;

  let wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

  // Stop watching for new windows
  wm.removeListener(windowListener);
}

function install(aData, aReason) { }
function uninstall(aData, aReason) { }

