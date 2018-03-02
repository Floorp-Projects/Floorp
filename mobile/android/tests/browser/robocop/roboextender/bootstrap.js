
ChromeUtils.import("resource://gre/modules/Messaging.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

function loadIntoWindow(window) {}
function unloadFromWindow(window) {}

/*
 bootstrap.js API
*/
var windowListener = {
  onOpenWindow: function(aWindow) {
    // Wait for the window to finish loading
    let domWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    domWindow.addEventListener("load", function() {
      if (domWindow) {
        domWindow.addEventListener("scroll", function(e) {
          let message = {
            type: "Robocop:Scroll",
            y: XPCNativeWrapper.unwrap(e.target).documentElement.scrollTop,
            height: XPCNativeWrapper.unwrap(e.target).documentElement.scrollHeight,
            cheight: XPCNativeWrapper.unwrap(e.target).documentElement.clientHeight,
          };
          EventDispatcher.instance.sendRequest(message);
        });
      }
    }, {once: true});
  },
  onCloseWindow: function(aWindow) { },
};

function startup(aData, aReason) {
  // Load into any new windows
  Services.wm.addListener(windowListener);
  EventDispatcher.instance.registerListener(function(event, data, callback) {
      dump("Robocop:Quit received -- requesting quit");
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  }, "Robocop:Quit");
}

function shutdown(aData, aReason) {
  // When the application is shutting down we normally don't have to clean up any UI changes
  if (aReason == APP_SHUTDOWN) return;

  // Stop watching for new windows
  Services.wm.removeListener(windowListener);
}

function install(aData, aReason) { }
function uninstall(aData, aReason) { }
