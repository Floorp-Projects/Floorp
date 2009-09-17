Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

var win = null;

function WindowListener(url) {
  this.url = url;

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  wm.addListener(this);
}

WindowListener.prototype = {
  url: null,

  onWindowTitleChange: function(window, title) {
  },

  onOpenWindow: function(window) {
    var domwindow = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                          .getInterface(Components.interfaces.nsIDOMWindowInternal);
    var self = this;
    domwindow.addEventListener("load", function() {
      self.windowLoad(domwindow);
    }, false);
  },

  // Window open handling
  windowLoad: function(window) {
    // Allow any other load handlers to execute
    var self = this;
    executeSoon(function() { self.windowReady(window); } );
  },

  windowReady: function(win) {
    is(win.document.location.href, this.url, "Should have seen the right window");
    win.document.documentElement.acceptDialog();
  },

  onCloseWindow: function(window) {
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.removeListener(this);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIWindowMediatorListener) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

function test() {
  is(LightweightThemeManager.usedThemes.length, 0, "Should be no themes there");
  ok(LightweightThemeManager.currentTheme == null, "Should not have a theme selected");

  // Load up some lightweight themes
  LightweightThemeManager.currentTheme = {
    "id":"2",
    "name":"Dirty Red",
    "accentcolor":"#ffffff",
    "textcolor":"#ffa0a0",
    "description":null,
    "author":"Mozilla",
    "headerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "footerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "previewURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "iconURL":null
  };
  LightweightThemeManager.currentTheme = {
    "id":"1",
    "name":"Abstract Black",
    "accentcolor":"#ffffff",
    "textcolor":"#000000",
    "description":null,
    "author":"Mozilla",
    "headerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "footerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "previewURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "iconURL":null
  };
  LightweightThemeManager.currentTheme = {
    "id":"3",
    "name":"Morose Mauve",
    "accentcolor":"#ffffff",
    "textcolor":"#e0b0ff",
    "description":null,
    "author":"Mozilla",
    "headerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "footerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "iconURL":null
  };
  is(LightweightThemeManager.usedThemes.length, 3, "Should be all the themes there");
  ok(LightweightThemeManager.currentTheme != null, "Should have selected a theme");
  is(LightweightThemeManager.currentTheme.id, 3, "Should have selected the right theme");

  win = window.openDialog("chrome://mozapps/content/extensions/extensions.xul", "",
                          "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable");
  win.addEventListener("load", function() { executeSoon(loaded); }, false);
  waitForExplicitFinish();
}

function loaded() {
  win.showView("themes");
  is(win.gExtensionsView.childNodes.length,
     LightweightThemeManager.usedThemes.length + 2,
     "Should be all the lightweight themes and the default theme and the template");
  is(win.gExtensionsView.childNodes[1].getAttribute("addonID"), 1,
     "Themes should be in the right order");
  is(win.gExtensionsView.childNodes[2].getAttribute("addonID"), "{972ce4c6-7e08-4474-a285-3208198ce6fd}",
     "Themes should be in the right order");
  is(win.gExtensionsView.childNodes[3].getAttribute("addonID"), 2,
     "Themes should be in the right order");
  is(win.gExtensionsView.childNodes[4].getAttribute("addonID"), 3,
     "Themes should be in the right order");

  var selected = win.gExtensionsView.selectedItem;
  is(selected.getAttribute("addonID"),
     LightweightThemeManager.currentTheme.id,
     "Should have selected the current theme");
  is(win.document.getElementById("previewImageDeck").selectedIndex, 1,
     "Should be no preview to display");
  var btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_useTheme");
  ok(btn.disabled, "Should not be able to switch to the current theme");

  selected = win.gExtensionsView.selectedItem = win.gExtensionsView.childNodes[1];
  is(win.document.getElementById("previewImageDeck").selectedIndex, 2,
     "Should be a preview to display");
  btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_useTheme");
  ok(!btn.disabled, "Should be able to switch to a different lightweight theme");
  EventUtils.synthesizeMouse(btn, btn.boxObject.width / 2, btn.boxObject.height / 2, {}, win);
  is(LightweightThemeManager.currentTheme.id, 1,
     "Should have changed theme");
  // A list rebuild happens so get the selected item again
  selected = win.gExtensionsView.selectedItem;
  btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_useTheme");
  ok(btn.disabled, "Should not be able to switch to the current theme");

  selected = win.gExtensionsView.selectedItem = win.gExtensionsView.childNodes[2];
  btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_useTheme");
  ok(!btn.disabled, "Should be able to switch to the default theme");
  EventUtils.synthesizeMouse(btn, btn.boxObject.width / 2, btn.boxObject.height / 2, {}, win);
  is(LightweightThemeManager.currentTheme, null,
     "Should have disabled lightweight themes");
  ok(btn.disabled, "Should not be able to switch to the current theme");

  selected = win.gExtensionsView.selectedItem = win.gExtensionsView.childNodes[3];
  btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_useTheme");
  ok(!btn.disabled, "Should be able to switch to a different lightweight theme");
  EventUtils.synthesizeMouse(btn, btn.boxObject.width / 2, btn.boxObject.height / 2, {}, win);
  is(LightweightThemeManager.currentTheme.id, 2,
     "Should have changed theme");
  // A list rebuild happens so get the selected item again
  selected = win.gExtensionsView.selectedItem;
  btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_useTheme");
  ok(btn.disabled, "Should not be able to switch to the current theme");
  btn = win.document.getAnonymousElementByAttribute(selected, "command", "cmd_uninstall");
  ok(!btn.disabled, "Should be able to uninstall a lightweight theme");
  new WindowListener("chrome://mozapps/content/extensions/list.xul");
  EventUtils.synthesizeMouse(btn, btn.boxObject.width / 2, btn.boxObject.height / 2, {}, win);
  is(LightweightThemeManager.currentTheme, null,
     "Should have turned off the lightweight theme");
  is(LightweightThemeManager.usedThemes.length, 2, "Should have removed the theme");
  is(win.gExtensionsView.childNodes.length,
     LightweightThemeManager.usedThemes.length + 2,
     "Should have updated the list");

  LightweightThemeManager.currentTheme = {
    "id":"2",
    "name":"Dirty Red",
    "accentcolor":"#ffffff",
    "textcolor":"#ffa0a0",
    "description":null,
    "author":"Mozilla",
    "headerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "footerURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "previewURL":"http://example.com/browser/toolkit/mozapps/extensions/test/blank.png",
    "iconURL":null
  };
  is(LightweightThemeManager.usedThemes.length, 3, "Should have added the theme");
  is(win.gExtensionsView.childNodes.length,
     LightweightThemeManager.usedThemes.length + 2,
     "Should have updated the list");

  win.close();
  endTest();
}

function endTest() {
  var themes = LightweightThemeManager.usedThemes;
  themes.forEach(function(theme) {
    LightweightThemeManager.forgetUsedTheme(theme.id);
  });
  is(LightweightThemeManager.usedThemes.length, 0, "Should be no themes left");
  ok(LightweightThemeManager.currentTheme == null, "Should be no theme selected");
  finish();
}
