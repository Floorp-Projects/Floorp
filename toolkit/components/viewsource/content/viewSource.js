// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/ViewSourceBrowser.jsm");

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
].forEach(function([name, id]) {
  window.__defineGetter__(name, function() {
    var element = document.getElementById(id);
    if (!element)
      return null;
    delete window[name];
    return window[name] = element;
  });
});

/**
 * ViewSourceChrome is the primary interface for interacting with
 * the view source browser from a self-contained window.  It extends
 * ViewSourceBrowser with additional things needed inside the special window.
 *
 * It initializes itself on script load.
 */
function ViewSourceChrome() {
  ViewSourceBrowser.call(this);
}

ViewSourceChrome.prototype = {
  __proto__: ViewSourceBrowser.prototype,

  /**
   * The <browser> that will be displaying the view source content.
   */
  get browser() {
    return gBrowser;
  },

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
  messages: ViewSourceBrowser.prototype.messages.concat([
    "ViewSource:SourceLoaded",
    "ViewSource:SourceUnloaded",
    "ViewSource:Close",
    "ViewSource:OpenURL",
    "ViewSource:UpdateStatus",
    "ViewSource:ContextMenuOpening",
  ]),

  /**
   * This called via ViewSourceBrowser's constructor.  This should be called as
   * soon as the script loads.  When this function executes, we can assume the
   * DOM content has not yet loaded.
   */
  init() {
    this.mm.loadFrameScript("chrome://global/content/viewSource-content.js", true);

    this.shouldWrap = Services.prefs.getBoolPref("view_source.wrap_long_lines");
    this.shouldHighlight =
      Services.prefs.getBoolPref("view_source.syntax_highlight");

    addEventListener("load", this);
    addEventListener("unload", this);
    addEventListener("AppCommand", this, true);
    addEventListener("MozSwipeGesture", this, true);

    ViewSourceBrowser.prototype.init.call(this);
  },

  /**
   * This should be called when the window is closing. This function should
   * clean up event and message listeners.
   */
  uninit() {
    ViewSourceBrowser.prototype.uninit.call(this);

    // "load" event listener is removed in its handler, to
    // ensure we only fire it once.
    removeEventListener("unload", this);
    removeEventListener("AppCommand", this, true);
    removeEventListener("MozSwipeGesture", this, true);
    gContextMenu.removeEventListener("popupshowing", this);
    gContextMenu.removeEventListener("popuphidden", this);
    Services.els.removeSystemEventListener(this.browser, "dragover", this,
                                           true);
    Services.els.removeSystemEventListener(this.browser, "drop", this, true);
  },

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(message) {
    let data = message.data;

    switch (message.name) {
      // Begin messages from super class
      case "ViewSource:PromptAndGoToLine":
        this.promptAndGoToLine();
        break;
      case "ViewSource:GoToLine:Success":
        this.onGoToLineSuccess(data.lineNumber);
        break;
      case "ViewSource:GoToLine:Failed":
        this.onGoToLineFailed();
        break;
      case "ViewSource:StoreWrapping":
        this.storeWrapping(data.state);
        break;
      case "ViewSource:StoreSyntaxHighlighting":
        this.storeSyntaxHighlighting(data.state);
        break;
      // End messages from super class
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
      case "ViewSource:UpdateStatus":
        this.updateStatus(data.label);
        break;
      case "ViewSource:ContextMenuOpening":
        this.onContextMenuOpening(data.isLink, data.isEmail, data.href);
        if (this.browser.isRemoteBrowser) {
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
    switch (event.type) {
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
    return !this.browser.hasAttribute("disablehistory");
  },

  /**
   * Getter for the message manager used to communicate with the view source
   * browser.
   *
   * In this window version of view source, we use the window message manager
   * for loading scripts and listening for messages so that if we switch
   * remoteness of the browser (which we might do if we're attempting to load
   * the document source out of the network cache), we automatically re-load
   * the frame script.
   */
  get mm() {
    return window.messageManager;
  },

  /**
   * Getter for the nsIWebNavigation of the view source browser.
   */
  get webNav() {
    return this.browser.webNavigation;
  },

  /**
   * Send the browser forward in its history.
   */
  goForward() {
    this.browser.goForward();
  },

  /**
   * Send the browser backward in its history.
   */
  goBack() {
    this.browser.goBack();
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

    Services.els.addSystemEventListener(this.browser, "dragover", this, true);
    Services.els.addSystemEventListener(this.browser, "drop", this, true);

    if (!this.historyEnabled) {
      // Disable the BACK and FORWARD commands and hide the related menu items.
      let viewSourceNavigation = document.getElementById("viewSourceNavigation");
      if (viewSourceNavigation) {
        viewSourceNavigation.setAttribute("disabled", "true");
        viewSourceNavigation.setAttribute("hidden", "true");
      }
    }

    // We require the first argument to do any loading of source.
    // otherwise, we're done.
    if (!window.arguments[0]) {
      return undefined;
    }

    if (typeof window.arguments[0] == "string") {
      // We're using the deprecated API
      return this._loadViewSourceDeprecated(window.arguments);
    }

    // We're using the modern API, which allows us to view the
    // source of documents from out of process browsers.
    let args = window.arguments[0];

    // viewPartialSource.js will take care of loading the content in partial mode.
    if (!args.partial) {
      this.loadViewSource(args);
    }

    return undefined;
  },

  /**
   * This is the deprecated API for viewSource.xul, for old-timer consumers.
   * This API might eventually go away.
   */
  _loadViewSourceDeprecated(aArguments) {
    Deprecated.warning("The arguments you're passing to viewSource.xul " +
                       "are using an out-of-date API.",
                       "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
    // Parse the 'arguments' supplied with the dialog.
    //    arg[0] - URL string.
    //    arg[1] - Charset value in the form 'charset=xxx'.
    //    arg[2] - Page descriptor used to load content from the cache.
    //    arg[3] - Line number to go to.
    //    arg[4] - Whether charset was forced by the user

    if (aArguments[2]) {
      let pageDescriptor = aArguments[2];
      if (Cu.isCrossProcessWrapper(pageDescriptor)) {
        throw new Error("Cannot pass a CPOW as the page descriptor to viewSource.xul.");
      }
    }

    if (this.browser.isRemoteBrowser) {
      throw new Error("Deprecated view source API should not use a remote browser.");
    }

    let forcedCharSet;
    if (aArguments[4] && aArguments[1].startsWith("charset=")) {
      forcedCharSet = aArguments[1].split("=")[1];
    }

    this.sendAsyncMessage("ViewSource:LoadSourceDeprecated", {
      URL: aArguments[0],
      lineNumber: aArguments[3],
      forcedCharSet,
    }, {
      pageDescriptor: aArguments[2],
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

    this.browser.focus();
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
      this.sendAsyncMessage("ViewSource:SetCharacterSet", {
        charset,
        doPageLoad: this.historyEnabled,
      });

      if (!this.historyEnabled) {
        this.browser
            .reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
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
    clipboard.copyString(this.contextMenuData.href);
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
    if (types.includes("text/x-moz-text-internal") && !types.includes("text/plain")) {
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
    this.sendAsyncMessage("ViewSource:LoadSource", { URL });
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
   * Called when the frame script reports that a line was successfully gotten
   * to.
   *
   * @param lineNumber
   *        The line number that we successfully got to.
   */
  onGoToLineSuccess(lineNumber) {
    ViewSourceBrowser.prototype.onGoToLineSuccess.call(this, lineNumber);
    document.getElementById("statusbar-line-col").label =
      gViewSourceBundle.getFormattedString("statusBarLineCol", [lineNumber, 1]);
  },

  /**
   * Reloads the browser, bypassing the network cache.
   */
  reload() {
    this.browser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
                                 Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  },

  /**
   * Closes the view source window.
   */
  close() {
    window.close();
  },

  /**
   * Called when the user clicks on the "Wrap Long Lines" menu item.
   */
  toggleWrapping() {
    this.shouldWrap = !this.shouldWrap;
    this.sendAsyncMessage("ViewSource:ToggleWrapping");
  },

  /**
   * Called when the user clicks on the "Syntax Highlighting" menu item.
   */
  toggleSyntaxHighlighting() {
    this.shouldHighlight = !this.shouldHighlight;
    this.sendAsyncMessage("ViewSource:ToggleSyntaxHighlighting");
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
   * @param remoteType
   *        The type of remote browser process.
   */
  updateBrowserRemoteness(shouldBeRemote, remoteType) {
    if (this.browser.isRemoteBrowser == shouldBeRemote &&
        this.browser.remoteType == remoteType) {
      return;
    }

    let parentNode = this.browser.parentNode;
    let nextSibling = this.browser.nextSibling;

    // XX Removing and re-adding the browser from and to the DOM strips its
    // XBL properties. Save and restore relatedBrowser. Note that when we
    // restore relatedBrowser, there won't yet be a binding or setter. This
    // works in conjunction with the hack in <xul:browser>'s constructor to
    // re-get the weak reference to it.
    let relatedBrowser = this.browser.relatedBrowser;

    this.browser.remove();
    if (shouldBeRemote) {
      this.browser.setAttribute("remote", "true");
      this.browser.setAttribute("remoteType", remoteType);
    } else {
      this.browser.removeAttribute("remote");
      this.browser.removeAttribute("remoteType");
    }

    this.browser.relatedBrowser = relatedBrowser;

    // If nextSibling was null, this will put the browser at
    // the end of the list.
    parentNode.insertBefore(this.browser, nextSibling);

    if (shouldBeRemote) {
      // We're going to send a message down to the remote browser
      // to load the source content - however, in order for the
      // contentWindowAsCPOW and contentDocumentAsCPOW values on
      // the remote browser to be set, we must set up the
      // RemoteWebProgress, which is lazily loaded. We only need
      // contentWindowAsCPOW for the printing support, and this
      // should go away once bug 1146454 is fixed, since we can
      // then just pass the outerWindowID of the this.browser to
      // PrintUtils.
      this.browser.webProgress;
    }
  },
};

var viewSourceChrome = new ViewSourceChrome();

/**
 * PrintUtils uses this to make Print Preview work.
 */
var PrintPreviewListener = {
  _ppBrowser: null,

  getPrintPreviewBrowser() {
    if (!this._ppBrowser) {
      this._ppBrowser = document.createElement("browser");
      this._ppBrowser.setAttribute("flex", "1");
      this._ppBrowser.setAttribute("type", "content");
    }

    if (gBrowser.isRemoteBrowser) {
      this._ppBrowser.setAttribute("remote", "true");
    } else {
      this._ppBrowser.removeAttribute("remote");
    }

    let findBar = document.getElementById("FindToolbar");
    document.getElementById("appcontent")
            .insertBefore(this._ppBrowser, findBar);

    return this._ppBrowser;
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
    this._ppBrowser.remove();
    gBrowser.collapsed = false;
    document.getElementById("viewSource-toolbox").hidden = false;
  },

  activateBrowser(browser) {
    browser.docShellIsActive = true;
  },
};

// viewZoomOverlay.js uses this
function getBrowser() {
  return gBrowser;
}

this.__defineGetter__("gPageLoader", function() {
  var webnav = viewSourceChrome.webNav;
  if (!webnav)
    return null;
  delete this.gPageLoader;
  this.gPageLoader = (webnav instanceof Ci.nsIWebPageDescriptor) ? webnav
                                                                 : null;
  return this.gPageLoader;
});

// Strips the |view-source:| for internalSave()
function ViewSourceSavePage() {
  internalSave(gBrowser.currentURI.spec.replace(/^view-source:/i, ""),
               null, null, null, null, null, "SaveLinkTitle",
               null, null, gBrowser.contentDocumentAsCPOW, null,
               gPageLoader);
}

// Below are old deprecated functions and variables left behind for
// compatibility reasons. These will be removed soon via bug 1159293.

this.__defineGetter__("gLastLineFound", function() {
  Deprecated.warning("gLastLineFound is deprecated - please use " +
                     "viewSourceChrome.lastLineFound instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  return viewSourceChrome.lastLineFound;
});

function onLoadViewSource() {
  Deprecated.warning("onLoadViewSource() is deprecated - please use " +
                     "viewSourceChrome.onXULLoaded() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.onXULLoaded();
}

function isHistoryEnabled() {
  Deprecated.warning("isHistoryEnabled() is deprecated - please use " +
                     "viewSourceChrome.historyEnabled instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  return viewSourceChrome.historyEnabled;
}

function ViewSourceClose() {
  Deprecated.warning("ViewSourceClose() is deprecated - please use " +
                     "viewSourceChrome.close() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.close();
}

function ViewSourceReload() {
  Deprecated.warning("ViewSourceReload() is deprecated - please use " +
                     "viewSourceChrome.reload() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.reload();
}

function getWebNavigation() {
  Deprecated.warning("getWebNavigation() is deprecated - please use " +
                     "viewSourceChrome.webNav instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  // The original implementation returned null if anything threw during
  // the getting of the webNavigation.
  try {
    return viewSourceChrome.webNav;
  } catch (e) {
    return null;
  }
}

function viewSource(url) {
  Deprecated.warning("viewSource() is deprecated - please use " +
                     "viewSourceChrome.loadURL() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.loadURL(url);
}

function ViewSourceGoToLine() {
  Deprecated.warning("ViewSourceGoToLine() is deprecated - please use " +
                     "viewSourceChrome.promptAndGoToLine() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.promptAndGoToLine();
}

function goToLine(line) {
  Deprecated.warning("goToLine() is deprecated - please use " +
                     "viewSourceChrome.goToLine() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.goToLine(line);
}

function BrowserForward(aEvent) {
  Deprecated.warning("BrowserForward() is deprecated - please use " +
                     "viewSourceChrome.goForward() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.goForward();
}

function BrowserBack(aEvent) {
  Deprecated.warning("BrowserBack() is deprecated - please use " +
                     "viewSourceChrome.goBack() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.goBack();
}

function UpdateBackForwardCommands() {
  Deprecated.warning("UpdateBackForwardCommands() is deprecated - please use " +
                     "viewSourceChrome.updateCommands() instead.",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/View_Source_for_XUL_Applications");
  viewSourceChrome.updateCommands();
}
