// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CharsetMenu",
  "resource://gre/modules/CharsetMenu.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

[
  ["gBrowser",          "content"],
  ["gViewSourceBundle", "viewSourceBundle"],
  ["gContextMenu",      "viewSourceContextMenu"]
].forEach(function ([name, id]) {
  window.__defineGetter__(name, function () {
    var element = document.getElementById(id);
    if (!element)
      return null;
    delete window[name];
    return window[name] = element;
  });
});

/**
 * ViewSourceChrome is the primary interface for interacting with
 * the view source browser. It initializes itself on script load.
 */
let ViewSourceChrome = {
  /**
   * Holds the value of the last line found via the "Go to line"
   * command, to pre-populate the prompt the next time it is
   * opened.
   */
  lastLineFound: null,

  /**
   * The context menu, when opened from the content process, sends
   * up a chunk of serialized data describing the items that the
   * context menu is being opened on. This allows us to avoid using
   * CPOWs.
   */
  contextMenuData: {},

  /**
   * These are the messages that ViewSourceChrome will listen for
   * from the frame script it injects. Any message names added here
   * will automatically have ViewSourceChrome listen for those messages,
   * and remove the listeners on teardown.
   */
  messages: [
    "ViewSource:SourceLoaded",
    "ViewSource:SourceUnloaded",
    "ViewSource:Close",
    "ViewSource:OpenURL",
    "ViewSource:GoToLine:Success",
    "ViewSource:GoToLine:Failed",
    "ViewSource:UpdateStatus",
    "ViewSource:ContextMenuOpening",
  ],

  /**
   * This should be called as soon as the script loads. When this function
   * executes, we can assume the DOM content has not yet loaded.
   */
  init() {
    // We use the window message manager so that if we switch remoteness of the
    // browser (which we might do if we're attempting to load the document
    // source out of the network cache), we automatically re-load the frame
    // script.
    let wMM = window.messageManager;
    wMM.loadFrameScript("chrome://global/content/viewSource-content.js", true);
    this.messages.forEach((msgName) => {
      wMM.addMessageListener(msgName, this);
    });

    this.shouldWrap = Services.prefs.getBoolPref("view_source.wrap_long_lines");
    this.shouldHighlight =
      Services.prefs.getBoolPref("view_source.syntax_highlight");

    addEventListener("load", this);
    addEventListener("unload", this);
    addEventListener("AppCommand", this, true);
    addEventListener("MozSwipeGesture", this, true);
  },

  /**
   * This should be called when the window is closing. This function should
   * clean up event and message listeners.
   */
  uninit() {
    let wMM = window.messageManager;
    this.messages.forEach((msgName) => {
      wMM.removeMessageListener(msgName, this);
    });

    // "load" event listener is removed in its handler, to
    // ensure we only fire it once.
    removeEventListener("unload", this);
    removeEventListener("AppCommand", this, true);
    removeEventListener("MozSwipeGesture", this, true);
    gContextMenu.removeEventListener("popupshowing", this);
    gContextMenu.removeEventListener("popuphidden", this);
    Services.els.removeSystemEventListener(gBrowser, "dragover", this, true);
    Services.els.removeSystemEventListener(gBrowser, "drop", this, true);
  },

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(message) {
    let data = message.data;

    switch(message.name) {
      case "ViewSource:SourceLoaded":
        this.onSourceLoaded();
        break;
      case "ViewSource:SourceUnloaded":
        this.onSourceUnloaded();
        break;
      case "ViewSource:Close":
        this.close();
        break;
      case "ViewSource:OpenURL":
        this.openURL(data.URL);
        break;
      case "ViewSource:GoToLine:Failed":
        this.onGoToLineFailed();
        break;
      case "ViewSource:GoToLine:Success":
        this.onGoToLineSuccess(data.lineNumber);
        break;
      case "ViewSource:UpdateStatus":
        this.updateStatus(data.label);
        break;
      case "ViewSource:ContextMenuOpening":
        this.onContextMenuOpening(data.isLink, data.isEmail, data.href);
        if (gBrowser.isRemoteBrowser) {
          this.openContextMenu(data.screenX, data.screenY);
        }
        break;
    }
  },

  /**
   * Any events should get handled here, and should get dispatched to
   * a specific function for the event type.
   */
  handleEvent(event) {
    switch(event.type) {
      case "unload":
        this.uninit();
        break;
      case "load":
        this.onXULLoaded();
        break;
      case "AppCommand":
        this.onAppCommand(event);
        break;
      case "MozSwipeGesture":
        this.onSwipeGesture(event);
        break;
      case "popupshowing":
        this.onContextMenuShowing(event);
        break;
      case "popuphidden":
        this.onContextMenuHidden(event);
        break;
      case "dragover":
        this.onDragOver(event);
        break;
      case "drop":
        this.onDrop(event);
        break;
    }
  },

  /**
   * Getter that returns whether or not the view source browser
   * has history enabled on it.
   */
  get historyEnabled() {
    return !gBrowser.hasAttribute("disablehistory");
  },

  /**
   * Getter for the message manager of the view source browser.
   */
  get mm() {
    return gBrowser.messageManager;
  },

  /**
   * Getter for the nsIWebNavigation of the view source browser.
   */
  get webNav() {
    return gBrowser.webNavigation;
  },

  /**
   * Send the browser forward in its history.
   */
  goForward() {
    gBrowser.goForward();
  },

  /**
   * Send the browser backward in its history.
   */
  goBack() {
    gBrowser.goBack();
  },

  /**
   * This should be called once when the DOM has finished loading. Here we
   * set the state of various menu items, and add event listeners to
   * DOM nodes.
   *
   * This is also the place where we handle any arguments that have been
   * passed to viewSource.xul.
   *
   * Modern consumers should pass a single object argument to viewSource.xul:
   *
   *   URL (required):
   *     A string URL for the page we'd like to view the source of.
   *   browser:
   *     The browser containing the document that we would like to view the
   *     source of. This argument is optional if outerWindowID is not passed.
   *   outerWindowID (optional):
   *     The outerWindowID of the content window containing the document that
   *     we want to view the source of. This is the only way of attempting to
   *     load the source out of the network cache.
   *   lineNumber (optional):
   *     The line number to focus on once the source is loaded.
   *
   * The deprecated API has the opener pass in a number of arguments:
   *
   * arg[0] - URL string.
   * arg[1] - Charset value string in the form 'charset=xxx'.
   * arg[2] - Page descriptor from nsIWebPageDescriptor used to load content
   *          from the cache.
   * arg[3] - Line number to go to.
   * arg[4] - Boolean for whether charset was forced by the user
   */
  onXULLoaded() {
    // This handler should only ever run the first time the XUL is loaded.
    removeEventListener("load", this);

    let wrapMenuItem = document.getElementById("menu_wrapLongLines");
    if (this.shouldWrap) {
      wrapMenuItem.setAttribute("checked", "true");
    }

    let highlightMenuItem = document.getElementById("menu_highlightSyntax");
    if (this.shouldHighlight) {
      highlightMenuItem.setAttribute("checked", "true");
    }

    gContextMenu.addEventListener("popupshowing", this);
    gContextMenu.addEventListener("popuphidden", this);

    Services.els.addSystemEventListener(gBrowser, "dragover", this, true);
    Services.els.addSystemEventListener(gBrowser, "drop", this, true);

    if (!this.historyEnabled) {
      // Disable the BACK and FORWARD commands and hide the related menu items.
      let viewSourceNavigation = document.getElementById("viewSourceNavigation");
      if (viewSourceNavigation) {
        viewSourceNavigation.setAttribute("disabled", "true");
        viewSourceNavigation.setAttribute("hidden", "true");
      }
    }

    // This will only work with non-remote browsers. See bug 1158377.
    gBrowser.droppedLinkHandler = function (event, url, name) {
      ViewSourceChrome.loadURL(url);
      event.preventDefault();
    };

    // We require the first argument to do any loading of source.
    // otherwise, we're done.
    if (!window.arguments[0]) {
      return;
    }

    if (typeof window.arguments[0] == "string") {
      // We're using the deprecated API
      return ViewSourceChrome._loadViewSourceDeprecated();
    }

    // We're using the modern API, which allows us to view the
    // source of documents from out of process browsers.
    let args = window.arguments[0];

    if (!args.URL) {
      throw new Error("Must supply a URL when opening view source.");
    }

    if (args.browser) {
      // If we're dealing with a remote browser, then the browser
      // for view source needs to be remote as well.
      this.updateBrowserRemoteness(args.browser.isRemoteBrowser);
    } else {
      if (args.outerWindowID) {
        throw new Error("Must supply the browser if passing the outerWindowID");
      }
    }

    this.mm.sendAsyncMessage("ViewSource:LoadSource", {
      URL: args.URL,
      outerWindowID: args.outerWindowID,
      lineNumber: args.lineNumber,
    });
  },

  /**
   * This is the deprecated API for viewSource.xul, for old-timer consumers.
   * This API might eventually go away.
   */
  _loadViewSourceDeprecated() {
    Deprecated.warning("The arguments you're passing to viewSource.xul " +
                       "are using an out-of-date API.",
                       "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
    // Parse the 'arguments' supplied with the dialog.
    //    arg[0] - URL string.
    //    arg[1] - Charset value in the form 'charset=xxx'.
    //    arg[2] - Page descriptor used to load content from the cache.
    //    arg[3] - Line number to go to.
    //    arg[4] - Whether charset was forced by the user

    if (window.arguments[3] == "selection" ||
        window.arguments[3] == "mathml") {
      // viewPartialSource.js will take care of loading the content.
      return;
    }

    if (window.arguments[2]) {
      let pageDescriptor = window.arguments[2];
      if (Cu.isCrossProcessWrapper(pageDescriptor)) {
        throw new Error("Cannot pass a CPOW as the page descriptor to viewSource.xul.");
      }
    }

    if (gBrowser.isRemoteBrowser) {
      throw new Error("Deprecated view source API should not use a remote browser.");
    }

    let forcedCharSet;
    if (window.arguments[4] && window.arguments[1].startsWith("charset=")) {
      forcedCharSet = window.arguments[1].split("=")[1];
    }

    gBrowser.messageManager.sendAsyncMessage("ViewSource:LoadSourceDeprecated", {
      URL: window.arguments[0],
      lineNumber: window.arguments[3],
      forcedCharSet,
    }, {
      pageDescriptor: window.arguments[2],
    });
  },

  /**
   * Handler for the AppCommand event.
   *
   * @param event
   *        The AppCommand event being handled.
   */
  onAppCommand(event) {
    event.stopPropagation();
    switch (event.command) {
      case "Back":
        this.goBack();
        break;
      case "Forward":
        this.goForward();
        break;
    }
  },

  /**
   * Handler for the MozSwipeGesture event.
   *
   * @param event
   *        The MozSwipeGesture event being handled.
   */
  onSwipeGesture(event) {
    event.stopPropagation();
    switch (event.direction) {
      case SimpleGestureEvent.DIRECTION_LEFT:
        this.goBack();
        break;
      case SimpleGestureEvent.DIRECTION_RIGHT:
        this.goForward();
        break;
      case SimpleGestureEvent.DIRECTION_UP:
        goDoCommand("cmd_scrollTop");
        break;
      case SimpleGestureEvent.DIRECTION_DOWN:
        goDoCommand("cmd_scrollBottom");
        break;
    }
  },

  /**
   * Called as soon as the frame script reports that some source
   * code has been loaded in the browser.
   */
  onSourceLoaded() {
    document.getElementById("cmd_goToLine").removeAttribute("disabled");

    if (this.historyEnabled) {
      this.updateCommands();
    }

    gBrowser.focus();
  },

  /**
   * Called as soon as the frame script reports that some source
   * code has been unloaded from the browser.
   */
  onSourceUnloaded() {
    // Disable "go to line" while reloading due to e.g. change of charset
    // or toggling of syntax highlighting.
    document.getElementById("cmd_goToLine").setAttribute("disabled", "true");
  },

  /**
   * Called by clicks on a menu populated by CharsetMenu.jsm to
   * change the selected character set.
   *
   * @param event
   *        The click event on a character set menuitem.
   */
  onSetCharacterSet(event) {
    if (event.target.hasAttribute("charset")) {
      let charset = event.target.getAttribute("charset");

      // If we don't have history enabled, we have to do a reload in order to
      // show the character set change. See bug 136322.
      this.mm.sendAsyncMessage("ViewSource:SetCharacterSet", {
        charset: charset,
        doPageLoad: this.historyEnabled,
      });

      if (this.historyEnabled) {
        gBrowser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
      }
    }
  },

  /**
   * Called from the frame script when the context menu is about to
   * open. This tells ViewSourceChrome things about the item that
   * the context menu is being opened on. This should be called before
   * the popupshowing event handler fires.
   */
  onContextMenuOpening(isLink, isEmail, href) {
    this.contextMenuData = { isLink, isEmail, href, isOpen: true };
  },

  /**
   * Event handler for the popupshowing event on the context menu.
   * This handler is responsible for setting the state on various
   * menu items in the context menu, and reads values that were sent
   * up from the frame script and stashed into this.contextMenuData.
   *
   * @param event
   *        The popupshowing event for the context menu.
   */
  onContextMenuShowing(event) {
    let copyLinkMenuItem = document.getElementById("context-copyLink");
    copyLinkMenuItem.hidden = !this.contextMenuData.isLink;

    let copyEmailMenuItem = document.getElementById("context-copyEmail");
    copyEmailMenuItem.hidden = !this.contextMenuData.isEmail;
  },

  /**
   * Called when the user chooses the "Copy Link" or "Copy Email"
   * menu items in the context menu. Copies the relevant selection
   * into the system clipboard.
   */
  onContextMenuCopyLinkOrEmail() {
    // It doesn't make any sense to call this if the context menu
    // isn't open...
    if (!this.contextMenuData.isOpen) {
      return;
    }

    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                      .getService(Ci.nsIClipboardHelper);
    clipboard.copyString(this.contextMenuData.href, document);
  },

  /**
   * Called when the context menu closes, and invalidates any data
   * that the frame script might have sent up about what the context
   * menu was opened on.
   */
  onContextMenuHidden(event) {
    this.contextMenuData = {
      isOpen: false,
    };
  },

  /**
   * Called when the user drags something over the content browser.
   */
  onDragOver(event) {
    // For drags that appear to be internal text (for example, tab drags),
    // set the dropEffect to 'none'. This prevents the drop even if some
    // other listener cancelled the event.
    let types = event.dataTransfer.types;
    if (types.contains("text/x-moz-text-internal") && !types.contains("text/plain")) {
        event.dataTransfer.dropEffect = "none";
        event.stopPropagation();
        event.preventDefault();
    }

    let linkHandler = Cc["@mozilla.org/content/dropped-link-handler;1"]
                        .getService(Ci.nsIDroppedLinkHandler);

    if (linkHandler.canDropLink(event, false)) {
      event.preventDefault();
    }
  },

  /**
   * Called twhen the user drops something onto the content browser.
   */
  onDrop(event) {
    if (event.defaultPrevented)
      return;

    let name = { };
    let linkHandler = Cc["@mozilla.org/content/dropped-link-handler;1"]
                        .getService(Ci.nsIDroppedLinkHandler);
    let uri;
    try {
      // Pass true to prevent the dropping of javascript:/data: URIs
      uri = linkHandler.dropLink(event, name, true);
    } catch (e) {
      return;
    }

    if (uri) {
      this.loadURL(uri);
    }
  },

  /**
   * For remote browsers, the contextmenu event is received in the
   * content process, and a message is sent up from the frame script
   * to ViewSourceChrome, but then it stops. The event does not bubble
   * up to the point that the popup is opened in the parent process.
   * ViewSourceChrome is responsible for opening up the context menu in
   * that case. This is called when we receive the contextmenu message
   * from the child, and we know that the browser is currently remote.
   *
   * @param screenX
   *        The screenX coordinate to open the popup at.
   * @param screenY
   *        The screenY coordinate to open the popup at.
   */
  openContextMenu(screenX, screenY) {
    gContextMenu.openPopupAtScreen(screenX, screenY, true);
  },

  /**
   * Loads the source of a URL. This will probably end up hitting the
   * network.
   *
   * @param URL
   *        A URL string to be opened in the view source browser.
   */
  loadURL(URL) {
    this.mm.sendAsyncMessage("ViewSource:LoadSource", { URL });
  },

  /**
   * Updates any commands that are dependant on command broadcasters.
   */
  updateCommands() {
    let backBroadcaster = document.getElementById("Browser:Back");
    let forwardBroadcaster = document.getElementById("Browser:Forward");

    if (this.webNav.canGoBack) {
      backBroadcaster.removeAttribute("disabled");
    } else {
      backBroadcaster.setAttribute("disabled", "true");
    }
    if (this.webNav.canGoForward) {
      forwardBroadcaster.removeAttribute("disabled");
    } else {
      forwardBroadcaster.setAttribute("disabled", "true");
    }
  },

  /**
   * Updates the status displayed in the status bar of the view source window.
   *
   * @param label
   *        The string to be displayed in the statusbar-lin-col element.
   */
  updateStatus(label) {
    let statusBarField = document.getElementById("statusbar-line-col");
    if (statusBarField) {
      statusBarField.label = label;
    }
  },

  /**
   * Opens the "Go to line" prompt for a user to hop to a particular line
   * of the source code they're viewing. This will keep prompting until the
   * user either cancels out of the prompt, or enters a valid line number.
   */
  promptAndGoToLine() {
    let input = { value: this.lastLineFound };

    let ok = Services.prompt.prompt(
        window,
        gViewSourceBundle.getString("goToLineTitle"),
        gViewSourceBundle.getString("goToLineText"),
        input,
        null,
        {value:0});

    if (!ok)
      return;

    let line = parseInt(input.value, 10);

    if (!(line > 0)) {
      Services.prompt.alert(window,
                            gViewSourceBundle.getString("invalidInputTitle"),
                            gViewSourceBundle.getString("invalidInputText"));
      this.promptAndGoToLine();
    } else {
      this.goToLine(line);
    }
  },

  /**
   * Go to a particular line of the source code. This act is asynchronous.
   *
   * @param lineNumber
   *        The line number to try to go to to.
   */
  goToLine(lineNumber) {
    this.mm.sendAsyncMessage("ViewSource:GoToLine", { lineNumber });
  },

  /**
   * Called when the frame script reports that a line was successfully gotten
   * to.
   *
   * @param lineNumber
   *        The line number that we successfully got to.
   */
  onGoToLineSuccess(lineNumber) {
    // We'll pre-populate the "Go to line" prompt with this value the next
    // time it comes up.
    this.lastLineFound = lineNumber;
    document.getElementById("statusbar-line-col").label =
      gViewSourceBundle.getFormattedString("statusBarLineCol", [lineNumber, 1]);
  },

  /**
   * Called when the frame script reports that we failed to go to a particular
   * line. This informs the user that their selection was likely out of range,
   * and then reprompts the user to try again.
   */
  onGoToLineFailed() {
    Services.prompt.alert(window,
                          gViewSourceBundle.getString("outOfRangeTitle"),
                          gViewSourceBundle.getString("outOfRangeText"));
    this.promptAndGoToLine();
  },

  /**
   * Reloads the browser, bypassing the network cache.
   */
  reload() {
    gBrowser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
                             Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  },

  /**
   * Closes the view source window.
   */
  close() {
    window.close();
  },

  /**
   * Called when the user clicks on the "Wrap Long Lines" menu item, and
   * flips the user preference for wrapping long lines in the view source
   * browser.
   */
  toggleWrapping() {
    this.shouldWrap = !this.shouldWrap;
    Services.prefs.setBoolPref("view_source.wrap_long_lines",
                               this.shouldWrap);
    this.mm.sendAsyncMessage("ViewSource:ToggleWrapping");
  },

  /**
   * Called when the user clicks on the "Syntax Highlighting" menu item, and
   * flips the user preference for wrapping long lines in the view source
   * browser.
   */
  toggleSyntaxHighlighting() {
    this.shouldHighlight = !this.shouldHighlight;
    // We can't flip this value in the child, since prefs are read-only there.
    // We flip it here, and then cause a reload in the child to make the change
    // occur.
    Services.prefs.setBoolPref("view_source.syntax_highlight",
                               this.shouldHighlight);
    this.mm.sendAsyncMessage("ViewSource:ToggleSyntaxHighlighting");
  },

  /**
   * Updates the "remote" attribute of the view source browser. This
   * will remove the browser from the DOM, and then re-add it in the
   * same place it was taken from.
   *
   * @param shouldBeRemote
   *        True if the browser should be made remote. If the browsers
   *        remoteness already matches this value, this function does
   *        nothing.
   */
  updateBrowserRemoteness(shouldBeRemote) {
    if (gBrowser.isRemoteBrowser == shouldBeRemote) {
      return;
    }

    let parentNode = gBrowser.parentNode;
    let nextSibling = gBrowser.nextSibling;

    gBrowser.remove();
    if (shouldBeRemote) {
      gBrowser.setAttribute("remote", "true");
    } else {
      gBrowser.removeAttribute("remote");
    }
    // If nextSibling was null, this will put the browser at
    // the end of the list.
    parentNode.insertBefore(gBrowser, nextSibling);

    if (shouldBeRemote) {
      // We're going to send a message down to the remote browser
      // to load the source content - however, in order for the
      // contentWindowAsCPOW and contentDocumentAsCPOW values on
      // the remote browser to be set, we must set up the
      // RemoteWebProgress, which is lazily loaded. We only need
      // contentWindowAsCPOW for the printing support, and this
      // should go away once bug 1146454 is fixed, since we can
      // then just pass the outerWindowID of the gBrowser to
      // PrintUtils.
      gBrowser.webProgress;
    }
  },
};

ViewSourceChrome.init();

/**
 * PrintUtils uses this to make Print Preview work.
 */
let PrintPreviewListener = {
  getPrintPreviewBrowser() {
    let browser = document.getElementById("ppBrowser");
    if (!browser) {
      browser = document.createElement("browser");
      browser.setAttribute("id", "ppBrowser");
      browser.setAttribute("flex", "1");
      browser.setAttribute("type", "content");

      let findBar = document.getElementById("FindToolbar");
      document.getElementById("appcontent")
              .insertBefore(browser, findBar);
    }

    return browser;
  },

  getSourceBrowser() {
    return gBrowser;
  },

  getNavToolbox() {
    return document.getElementById("appcontent");
  },

  onEnter() {
    let toolbox = document.getElementById("viewSource-toolbox");
    toolbox.hidden = true;
    gBrowser.collapsed = true;
  },

  onExit() {
    document.getElementById("ppBrowser").collapsed = true;
    gBrowser.collapsed = false;
    document.getElementById("viewSource-toolbox").hidden = false;
  },
};

// viewZoomOverlay.js uses this
function getBrowser() {
  return gBrowser;
}

this.__defineGetter__("gPageLoader", function () {
  var webnav = ViewSourceChrome.webNav;
  if (!webnav)
    return null;
  delete this.gPageLoader;
  this.gPageLoader = (webnav instanceof Ci.nsIWebPageDescriptor) ? webnav
                                                                 : null;
  return this.gPageLoader;
});

// Strips the |view-source:| for internalSave()
function ViewSourceSavePage()
{
  internalSave(gBrowser.currentURI.spec.replace(/^view-source:/i, ""),
               null, null, null, null, null, "SaveLinkTitle",
               null, null, gBrowser.contentDocumentAsCPOW, null,
               gPageLoader);
}

// Below are old deprecated functions and variables left behind for
// compatibility reasons. These will be removed soon via bug 1159293.

this.__defineGetter__("gLastLineFound", function () {
  Deprecated.warning("gLastLineFound is deprecated - please use " +
                     "ViewSourceChrome.lastLineFound instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  return ViewSourceChrome.lastLineFound;
});

function onLoadViewSource() {
  Deprecated.warning("onLoadViewSource() is deprecated - please use " +
                     "ViewSourceChrome.onXULLoaded() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.onXULLoaded();
}

function isHistoryEnabled() {
  Deprecated.warning("isHistoryEnabled() is deprecated - please use " +
                     "ViewSourceChrome.historyEnabled instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  return ViewSourceChrome.historyEnabled;
}

function ViewSourceClose() {
  Deprecated.warning("ViewSourceClose() is deprecated - please use " +
                     "ViewSourceChrome.close() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.close();
}

function ViewSourceReload() {
  Deprecated.warning("ViewSourceReload() is deprecated - please use " +
                     "ViewSourceChrome.reload() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.reload();
}

function getWebNavigation()
{
  Deprecated.warning("getWebNavigation() is deprecated - please use " +
                     "ViewSourceChrome.webNav instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  // The original implementation returned null if anything threw during
  // the getting of the webNavigation.
  try {
    return ViewSourceChrome.webNav;
  } catch (e) {
    return null;
  }
}

function viewSource(url) {
  Deprecated.warning("viewSource() is deprecated - please use " +
                     "ViewSourceChrome.loadURL() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.loadURL(url);
}

function ViewSourceGoToLine()
{
  Deprecated.warning("ViewSourceGoToLine() is deprecated - please use " +
                     "ViewSourceChrome.promptAndGoToLine() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.promptAndGoToLine();
}

function goToLine(line)
{
  Deprecated.warning("goToLine() is deprecated - please use " +
                     "ViewSourceChrome.goToLine() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.goToLine(line);
}

function BrowserForward(aEvent) {
  Deprecated.warning("BrowserForward() is deprecated - please use " +
                     "ViewSourceChrome.goForward() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.goForward();
}

function BrowserBack(aEvent) {
  Deprecated.warning("BrowserBack() is deprecated - please use " +
                     "ViewSourceChrome.goBack() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.goBack();
}

function UpdateBackForwardCommands() {
  Deprecated.warning("UpdateBackForwardCommands() is deprecated - please use " +
                     "ViewSourceChrome.updateCommands() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/ViewSource");
  ViewSourceChrome.updateCommands();
}
