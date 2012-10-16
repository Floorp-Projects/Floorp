/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests bug 567127 - Add install button to the add-ons manager

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

var gManagerWindow;
var gSawInstallNotification = false;

// This listens for the next opened window and checks it is of the right url.
// opencallback is called when the new window is fully loaded
// closecallback is called when the window is closed
function WindowOpenListener(url, opencallback, closecallback) {
  this.url = url;
  this.opencallback = opencallback;
  this.closecallback = closecallback;

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  wm.addListener(this);
}

WindowOpenListener.prototype = {
  url: null,
  opencallback: null,
  closecallback: null,
  window: null,
  domwindow: null,

  handleEvent: function(event) {
    is(this.domwindow.document.location.href, this.url, "Should have opened the correct window");

    this.domwindow.removeEventListener("load", this, false);
    // Allow any other load handlers to execute
    var self = this;
    executeSoon(function() { self.opencallback(self.domwindow); } );
  },

  onWindowTitleChange: function(window, title) {
  },

  onOpenWindow: function(window) {
    if (this.window)
      return;

    this.window = window;
    this.domwindow = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                           .getInterface(Components.interfaces.nsIDOMWindow);
    this.domwindow.addEventListener("load", this, false);
  },

  onCloseWindow: function(window) {
    if (this.window != window)
      return;

    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.removeListener(this);
    this.opencallback = null;
    this.window = null;
    this.domwindow = null;

    // Let the window close complete
    executeSoon(this.closecallback);
    this.closecallback = null;
  }
};


var gInstallNotificationObserver = {
  observe: function(aSubject, aTopic, aData) {
    var installInfo = aSubject.QueryInterface(Ci.amIWebInstallInfo);
    isnot(installInfo.originatingWindow, null, "Notification should have non-null originatingWindow");
    gSawInstallNotification = true;
    Services.obs.removeObserver(this, "addon-install-started");
  }
};


function test_confirmation(aWindow, aExpectedURLs) {
  var list = aWindow.document.getElementById("itemList");
  is(list.childNodes.length, aExpectedURLs.length, "Should be the right number of installs");

  for (let url of aExpectedURLs) {
    let found = false;
    for (let node of list.children) {
      if (node.url == url) {
        found = true;
        break;
      }
    }
    ok(found, "Should have seen " + url + " in the list");
  }

  aWindow.document.documentElement.cancelDialog();
}


function test() {
  waitForExplicitFinish();
  
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  is(gSawInstallNotification, true, "Should have seen addon-install-started notification.");

  MockFilePicker.cleanup();
  close_manager(gManagerWindow, function() {
    finish();
  });
}


add_test(function() {
  var filePaths = [
                   get_addon_file_url("browser_bug567127_1.xpi"),
                   get_addon_file_url("browser_bug567127_2.xpi")
                  ];
  MockFilePicker.returnFiles = filePaths.map(function(aPath) aPath.file);
  
  Services.obs.addObserver(gInstallNotificationObserver,
                           "addon-install-started", false);

  new WindowOpenListener(INSTALL_URI, function(aWindow) {
    test_confirmation(aWindow, filePaths.map(function(aPath) aPath.spec));
  }, run_next_test);

  gManagerWindow.gViewController.doCommand("cmd_installFromFile");
});
