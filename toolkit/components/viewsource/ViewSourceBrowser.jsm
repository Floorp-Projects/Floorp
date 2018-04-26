// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

const FRAME_SCRIPT = "chrome://global/content/viewSource-content.js";

var EXPORTED_SYMBOLS = ["ViewSourceBrowser"];

// Keep a set of browsers we've seen before, so we can load our frame script as
// needed into any new ones.
var gKnownBrowsers = new WeakSet();

/**
 * ViewSourceBrowser manages the view source <browser> from the chrome side.
 * It's companion frame script, viewSource-content.js, needs to be loaded as a
 * frame script into the browser being managed.
 *
 * For a view source tab (or some other non-window case), an instance of this is
 * created by viewSourceUtils.js to wrap the <browser>.  The frame script will
 * be loaded by this module at construction time.
 */
function ViewSourceBrowser(aBrowser) {
  this._browser = aBrowser;
  this.init();
}

ViewSourceBrowser.prototype = {
  /**
   * The <browser> that will be displaying the view source content.
   */
  get browser() {
    return this._browser;
  },

  /**
   * Holds the value of the last line found via the "Go to line"
   * command, to pre-populate the prompt the next time it is
   * opened.
   */
  lastLineFound: null,

  /**
   * These are the messages that ViewSourceBrowser will listen for
   * from the frame script it injects. Any message names added here
   * will automatically have ViewSourceBrowser listen for those messages,
   * and remove the listeners on teardown.
   */
  messages: [
    "ViewSource:PromptAndGoToLine",
    "ViewSource:GoToLine:Success",
    "ViewSource:GoToLine:Failed",
    "ViewSource:StoreWrapping",
    "ViewSource:StoreSyntaxHighlighting",
  ],

  /**
   * This should be called as soon as the script loads. When this function
   * executes, we can assume the DOM content has not yet loaded.
   */
  init() {
    this.messages.forEach((msgName) => {
      this.mm.addMessageListener(msgName, this);
    });

    this.loadFrameScript();
  },

  /**
   * This should be called when the window is closing. This function should
   * clean up event and message listeners.
   */
  uninit() {
    this.messages.forEach((msgName) => {
      this.mm.removeMessageListener(msgName, this);
    });
  },

  /**
   * For a new browser we've not seen before, load the frame script.
   */
  loadFrameScript() {
    // Check for a browser first. There won't be one for the window case
    // (still used by other applications like Thunderbird), as the element
    // does not exist until the XUL document loads.
    if (!this.browser) {
      return;
    }
    if (!gKnownBrowsers.has(this.browser)) {
      gKnownBrowsers.add(this.browser);
      this.mm.loadFrameScript(FRAME_SCRIPT, false);
    }
  },

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(message) {
    let data = message.data;

    switch (message.name) {
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
    }
  },

  /**
   * Getter for the message manager of the view source browser.
   */
  get mm() {
    return this.browser.messageManager;
  },

  /**
   * Send a message to the view source browser.
   */
  sendAsyncMessage(...args) {
    this.browser.messageManager.sendAsyncMessage(...args);
  },

  /**
   * A getter for the view source string bundle.
   */
  get bundle() {
    if (this._bundle) {
      return this._bundle;
    }
    return this._bundle = Services.strings.createBundle(BUNDLE_URL);
  },

  /**
   * Loads the source for a URL while applying some optional features if
   * enabled.
   *
   * For view source in a specific browser, this is manually called after
   * this object is constructed.
   *
   * This takes a single object argument containing:
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
   */
  loadViewSource({ URL, browser, outerWindowID, lineNumber }) {
    if (!URL) {
      throw new Error("Must supply a URL when opening view source.");
    }

    if (browser) {
      this.browser.sameProcessAsFrameLoader = browser.frameLoader;

      // If we're dealing with a remote browser, then the browser
      // for view source needs to be remote as well.
      this.updateBrowserRemoteness(browser.isRemoteBrowser, browser.remoteType);
    } else if (outerWindowID) {
      throw new Error("Must supply the browser if passing the outerWindowID");
    }

    this.sendAsyncMessage("ViewSource:LoadSource",
                          { URL, outerWindowID, lineNumber });
  },

  /**
   * Loads a view source selection showing the given view-source url and
   * highlight the selection.
   *
   * @param uri view-source uri to show
   * @param drawSelection true to highlight the selection
   * @param baseURI base URI of the original document
   */
  loadViewSourceFromSelection(URL, drawSelection, baseURI) {
    this.sendAsyncMessage("ViewSource:LoadSourceWithSelection",
                          { URL, drawSelection, baseURI });
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
    if (this.browser.isRemoteBrowser != shouldBeRemote ||
        this.browser.remoteType != remoteType) {
      // In this base case, where we are handed a <browser> someone else is
      // managing, we don't know for sure that it's safe to toggle remoteness.
      // For view source in a window, this is overridden to actually do the
      // flip if needed.
      throw new Error("View source browser's remoteness mismatch");
    }
  },

  /**
   * Opens the "Go to line" prompt for a user to hop to a particular line
   * of the source code they're viewing. This will keep prompting until the
   * user either cancels out of the prompt, or enters a valid line number.
   */
  promptAndGoToLine() {
    let input = { value: this.lastLineFound };
    let window = Services.wm.getMostRecentWindow(null);

    let ok = Services.prompt.prompt(
        window,
        this.bundle.GetStringFromName("goToLineTitle"),
        this.bundle.GetStringFromName("goToLineText"),
        input,
        null,
        {value: 0});

    if (!ok)
      return;

    let line = parseInt(input.value, 10);

    if (!(line > 0)) {
      Services.prompt.alert(window,
                            this.bundle.GetStringFromName("invalidInputTitle"),
                            this.bundle.GetStringFromName("invalidInputText"));
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
    this.sendAsyncMessage("ViewSource:GoToLine", { lineNumber });
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
  },

  /**
   * Called when the frame script reports that we failed to go to a particular
   * line. This informs the user that their selection was likely out of range,
   * and then reprompts the user to try again.
   */
  onGoToLineFailed() {
    let window = Services.wm.getMostRecentWindow(null);
    Services.prompt.alert(window,
                          this.bundle.GetStringFromName("outOfRangeTitle"),
                          this.bundle.GetStringFromName("outOfRangeText"));
    this.promptAndGoToLine();
  },

  /**
   * Update the wrapping pref based on the child's current state.
   * @param state
   *        Whether wrapping is currently enabled in the child.
   */
  storeWrapping(state) {
    Services.prefs.setBoolPref("view_source.wrap_long_lines", state);
  },

  /**
   * Update the syntax highlighting pref based on the child's current state.
   * @param state
   *        Whether syntax highlighting is currently enabled in the child.
   */
  storeSyntaxHighlighting(state) {
    Services.prefs.setBoolPref("view_source.syntax_highlight", state);
  },

};

/**
 * Helper to decide if a URI maps to view source content.
 * @param uri
 *        String containing the URI
 */
ViewSourceBrowser.isViewSource = function(uri) {
  return uri.startsWith("view-source:") ||
         (uri.startsWith("data:") && uri.includes("MathML"));
};
