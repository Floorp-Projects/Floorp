// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 sts=2 et tw=80: */
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["FinderParent"];

const kModalHighlightPref = "findbar.modalHighlight";
const kSoundEnabledPref = "accessibility.typeaheadfind.enablesound";
const kNotFoundSoundPref = "accessibility.typeaheadfind.soundURL";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "GetClipboardSearchString",
  "resource://gre/modules/Finder.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "Rect",
  "resource://gre/modules/Geometry.jsm"
);

const kPrefLetterboxing = "privacy.resistFingerprinting.letterboxing";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isLetterboxingEnabled",
  kPrefLetterboxing,
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isSoundEnabled",
  kSoundEnabledPref,
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "notFoundSoundURL",
  kNotFoundSoundPref,
  ""
);

function FinderParent(browser) {
  this._listeners = new Set();
  this._searchString = "";
  this._foundSearchString = null;
  this._lastFoundBrowsingContext = null;

  // The correct states of these will be updated when the findbar is opened.
  this._caseSensitive = false;
  this._entireWord = false;
  this._matchDiacritics = false;

  this.swapBrowser(browser);
}

let gSound = null;

FinderParent.prototype = {
  get browsingContext() {
    return this._browser.browsingContext;
  },

  get useRemoteSubframes() {
    return this._browser.ownerGlobal.docShell.nsILoadContext.useRemoteSubframes;
  },

  swapBrowser(aBrowser) {
    this._browser = aBrowser;
    // Ideally listeners would have removed themselves but that doesn't happen
    // right now
    this._listeners.clear();
  },

  addResultListener(aListener) {
    this._listeners.add(aListener);
  },

  removeResultListener(aListener) {
    this._listeners.delete(aListener);
  },

  callListeners(aCallback, aArgs) {
    for (let l of this._listeners) {
      // Don't let one callback throwing stop us calling the rest
      try {
        l[aCallback].apply(l, aArgs);
      } catch (e) {
        if (!l[aCallback]) {
          Cu.reportError(
            `Missing ${aCallback} callback on RemoteFinderListener`
          );
        } else {
          Cu.reportError(e);
        }
      }
    }
  },

  getLastFoundBrowsingContext(aList) {
    // If a search was already performed, returned the last
    // browsing context where the result was found. However,
    // ensure that this browsing context is still valid, and
    // if not, return null.
    if (aList.includes(this._lastFoundBrowsingContext)) {
      return this._lastFoundBrowsingContext;
    }

    this._lastFoundBrowsingContext = null;
    return null;
  },

  sendMessageToContext(aMessageName, aArgs = {}) {
    // If there is a last found browsing context, use that. Otherwise,
    // use the top-level browsing context.
    let browsingContext = null;
    if (this._lastFoundBrowsingContext) {
      let list = this.gatherBrowsingContexts(this.browsingContext);
      let lastBrowsingContext = this.getLastFoundBrowsingContext(list);
      if (lastBrowsingContext) {
        browsingContext = lastBrowsingContext;
      }
    }

    if (!browsingContext) {
      browsingContext = this.browsingContext;
    }

    let windowGlobal = browsingContext.currentWindowGlobal;
    if (windowGlobal) {
      let actor = windowGlobal.getActor("Finder");
      actor.sendAsyncMessage(aMessageName, aArgs);
    }
  },

  sendQueryToContext(aMessageName, aArgs, aBrowsingContext) {
    let windowGlobal = aBrowsingContext.currentWindowGlobal;
    if (windowGlobal) {
      let actor = windowGlobal.getActor("Finder");
      return actor.sendQuery(aMessageName, aArgs).then(
        result => result,
        r => {}
      );
    }

    return Promise.resolve({});
  },

  sendMessageToAllContexts(aMessageName, aArgs = {}) {
    let list = this.gatherBrowsingContexts(this.browsingContext);
    for (let browsingContext of list) {
      let windowGlobal = browsingContext.currentWindowGlobal;
      if (windowGlobal) {
        let actor = windowGlobal.getActor("Finder");
        actor.sendAsyncMessage(aMessageName, aArgs);
      }
    }
  },

  gatherBrowsingContexts(aBrowsingContext) {
    let list = [aBrowsingContext];

    for (let child of aBrowsingContext.children) {
      list.push(...this.gatherBrowsingContexts(child));
    }

    return list;
  },

  // If the modal highlighter is on, and there are no out-of-process child
  // frames, send a message only to the top-level frame and set the useSubFrames
  // flag, so that the finder iterator iterates over subframes. If there is
  // an out-of-process subframe, modal highlighting is disabled.
  needSubFrameSearch(aList) {
    let useSubFrames = false;

    let useModalHighlighter = Services.prefs.getBoolPref(kModalHighlightPref);
    let hasOutOfProcessChild = false;
    if (useModalHighlighter) {
      if (this.useRemoteSubframes) {
        return false;
      }

      for (let browsingContext of aList) {
        if (
          browsingContext != this.browsingContext &&
          browsingContext.currentWindowGlobal.isProcessRoot
        ) {
          hasOutOfProcessChild = true;
        }
      }

      if (!hasOutOfProcessChild) {
        aList.splice(0);
        aList.push(this.browsingContext);
        useSubFrames = true;
      }
    }

    return useSubFrames;
  },

  onResultFound(aResponse) {
    this._foundSearchString = aResponse.searchString;
    // The rect stops being a Geometry.jsm:Rect over IPC.
    if (aResponse.rect) {
      aResponse.rect = lazy.Rect.fromRect(aResponse.rect);
    }

    this.callListeners("onFindResult", [aResponse]);
  },

  get searchString() {
    return this._foundSearchString;
  },

  get clipboardSearchString() {
    return lazy.GetClipboardSearchString(this._browser.loadContext);
  },

  set caseSensitive(aSensitive) {
    this._caseSensitive = aSensitive;
    this.sendMessageToAllContexts("Finder:CaseSensitive", {
      caseSensitive: aSensitive,
    });
  },

  set entireWord(aEntireWord) {
    this._entireWord = aEntireWord;
    this.sendMessageToAllContexts("Finder:EntireWord", {
      entireWord: aEntireWord,
    });
  },

  set matchDiacritics(aMatchDiacritics) {
    this._matchDiacritics = aMatchDiacritics;
    this.sendMessageToAllContexts("Finder:MatchDiacritics", {
      matchDiacritics: aMatchDiacritics,
    });
  },

  async setSearchStringToSelection() {
    return this.setToSelection("Finder:SetSearchStringToSelection", false);
  },

  async getInitialSelection() {
    return this.setToSelection("Finder:GetInitialSelection", true);
  },

  async setToSelection(aMessage, aInitial) {
    let browsingContext = this.browsingContext;

    // Iterate over focused subframe descendants until one is found
    // that has the selection.
    let result;
    do {
      result = await this.sendQueryToContext(aMessage, {}, browsingContext);
      if (!result || !result.focusedChildBrowserContextId) {
        break;
      }

      browsingContext = BrowsingContext.get(
        result.focusedChildBrowserContextId
      );
    } while (browsingContext);

    if (result) {
      this.callListeners("onCurrentSelection", [result.selectedText, aInitial]);
    }

    return result;
  },

  async doFind(aFindNext, aArgs) {
    let rootBC = this.browsingContext;
    let highlightList = this.gatherBrowsingContexts(rootBC);

    let canPlayNotFoundSound =
      aArgs.searchString.length > this._searchString.length;

    this._searchString = aArgs.searchString;

    let initialBC = this.getLastFoundBrowsingContext(highlightList);
    if (!initialBC) {
      initialBC = rootBC;
      aFindNext = false;
    }

    // Make a copy of the list starting from the
    // browsing context that was last searched from. The original
    // list will be used for the highlighter where the search
    // order doesn't matter.
    let searchList = [];
    for (let c = 0; c < highlightList.length; c++) {
      if (highlightList[c] == initialBC) {
        searchList = highlightList.slice(c);
        searchList.push(...highlightList.slice(0, c));
        break;
      }
    }

    let mode = Ci.nsITypeAheadFind.FIND_INITIAL;
    if (aFindNext) {
      mode = aArgs.findBackwards
        ? Ci.nsITypeAheadFind.FIND_PREVIOUS
        : Ci.nsITypeAheadFind.FIND_NEXT;
    }
    aArgs.findAgain = aFindNext;

    aArgs.caseSensitive = this._caseSensitive;
    aArgs.matchDiacritics = this._matchDiacritics;
    aArgs.entireWord = this._entireWord;

    aArgs.useSubFrames = this.needSubFrameSearch(searchList);
    if (aArgs.useSubFrames) {
      // Use the single frame for the highlight list as well.
      highlightList = searchList;
      // The typeaheadfind component will play the sound in this case.
      canPlayNotFoundSound = false;
    }

    if (canPlayNotFoundSound) {
      this.initNotFoundSound();
    }

    // Add the initial browsing context twice to allow looping around.
    searchList = [...searchList, initialBC];

    if (aArgs.findBackwards) {
      searchList.reverse();
    }

    let response = null;
    let wrapped = false;
    let foundBC = null;

    for (let c = 0; c < searchList.length; c++) {
      let currentBC = searchList[c];
      aArgs.mode = mode;

      // A search has started for a different string, so
      // ignore further searches of the old string.
      if (this._searchString != aArgs.searchString) {
        return;
      }

      response = await this.sendQueryToContext("Finder:Find", aArgs, currentBC);

      // This can happen if the tab is closed while the find is in progress.
      if (!response) {
        break;
      }

      // If the search term was found, stop iterating.
      if (response.result != Ci.nsITypeAheadFind.FIND_NOTFOUND) {
        if (
          this._lastFoundBrowsingContext &&
          this._lastFoundBrowsingContext != currentBC
        ) {
          // If the new result is in a different frame than the previous result,
          // clear the result from the old frame. If it is the same frame, the
          // previous result will be cleared by the find component.
          this.removeSelection(true);
        }
        this._lastFoundBrowsingContext = currentBC;

        // Set the wrapped result flag if needed.
        if (wrapped) {
          response.result = Ci.nsITypeAheadFind.FIND_WRAPPED;
        }

        foundBC = currentBC;
        break;
      }

      if (aArgs.findBackwards && currentBC == rootBC) {
        wrapped = true;
      } else if (
        !aArgs.findBackwards &&
        c + 1 < searchList.length &&
        searchList[c + 1] == rootBC
      ) {
        wrapped = true;
      }

      mode = aArgs.findBackwards
        ? Ci.nsITypeAheadFind.FIND_LAST
        : Ci.nsITypeAheadFind.FIND_FIRST;
    }

    if (response) {
      response.useSubFrames = aArgs.useSubFrames;
      // Update the highlight in all browsing contexts. This needs to happen separately
      // once it is clear whether a match was found or not.
      this.updateHighlightAndMatchCount({
        list: highlightList,
        message: "Finder:UpdateHighlightAndMatchCount",
        args: response,
        foundBrowsingContextId: foundBC ? foundBC.id : -1,
        doHighlight: true,
        doMatchCount: true,
      });

      // Use the last result found.
      this.onResultFound(response);

      if (
        canPlayNotFoundSound &&
        response.result == Ci.nsITypeAheadFind.FIND_NOTFOUND &&
        !aFindNext &&
        !response.entireWord
      ) {
        this.playNotFoundSound();
      }
    }
  },

  fastFind(aSearchString, aLinksOnly, aDrawOutline) {
    this.doFind(false, {
      searchString: aSearchString,
      findBackwards: false,
      linksOnly: aLinksOnly,
      drawOutline: aDrawOutline,
    });
  },

  findAgain(aSearchString, aFindBackwards, aLinksOnly, aDrawOutline) {
    this.doFind(true, {
      searchString: aSearchString,
      findBackwards: aFindBackwards,
      linksOnly: aLinksOnly,
      drawOutline: aDrawOutline,
    });
  },

  highlight(aHighlight, aWord, aLinksOnly) {
    let list = this.gatherBrowsingContexts(this.browsingContext);
    let args = {
      highlight: aHighlight,
      linksOnly: aLinksOnly,
      searchString: aWord,
    };

    args.useSubFrames = this.needSubFrameSearch(list);

    let lastBrowsingContext = this.getLastFoundBrowsingContext(list);
    this.updateHighlightAndMatchCount({
      list,
      message: "Finder:Highlight",
      args,
      foundBrowsingContextId: lastBrowsingContext ? lastBrowsingContext.id : -1,
      doHighlight: true,
      doMatchCount: false,
    });
  },

  requestMatchesCount(aSearchString, aLinksOnly) {
    let list = this.gatherBrowsingContexts(this.browsingContext);
    let args = { searchString: aSearchString, linksOnly: aLinksOnly };

    args.useSubFrames = this.needSubFrameSearch(list);

    let lastBrowsingContext = this.getLastFoundBrowsingContext(list);
    this.updateHighlightAndMatchCount({
      list,
      message: "Finder:MatchesCount",
      args,
      foundBrowsingContextId: lastBrowsingContext ? lastBrowsingContext.id : -1,
      doHighlight: false,
      doMatchCount: true,
    });
  },

  updateHighlightAndMatchCount(options) {
    let promises = [];
    let found = options.args.result != Ci.nsITypeAheadFind.FIND_NOTFOUND;
    for (let browsingContext of options.list) {
      options.args.foundInThisFrame =
        options.foundBrowsingContextId != -1 &&
        found &&
        browsingContext.id == options.foundBrowsingContextId;

      // Don't wait for the result
      let promise = this.sendQueryToContext(
        options.message,
        options.args,
        browsingContext
      );
      promises.push(promise);
    }

    Promise.all(promises).then(responses => {
      if (options.doHighlight) {
        let sendNotification = false;
        let highlight = false;
        let found = false;
        for (let response of responses) {
          if (!response) {
            break;
          }

          sendNotification = true;
          if (response.found) {
            found = true;
          }
          highlight = response.highlight;
        }

        if (sendNotification) {
          this.callListeners("onHighlightFinished", [
            { searchString: options.args.searchString, highlight, found },
          ]);
        }
      }

      if (options.doMatchCount) {
        let sendNotification = false;
        let current = 0;
        let total = 0;
        let limit = 0;
        for (let response of responses) {
          // A null response can happen if another search was started
          // and this one became invalid.
          if (!response || !("total" in response)) {
            break;
          }

          sendNotification = true;

          if (
            options.args.useSubFrames ||
            (options.foundBrowsingContextId >= 0 &&
              response.browsingContextId == options.foundBrowsingContextId)
          ) {
            current = total + response.current;
          }
          total += response.total;
          limit = response.limit;
        }

        if (sendNotification) {
          this.callListeners("onMatchesCountResult", [
            { searchString: options.args.searchString, current, total, limit },
          ]);
        }
      }
    });
  },

  enableSelection() {
    this.sendMessageToContext("Finder:EnableSelection");
  },

  removeSelection(aKeepHighlight) {
    this.sendMessageToContext("Finder:RemoveSelection", {
      keepHighlight: aKeepHighlight,
    });
  },

  focusContent() {
    // Allow Finder listeners to cancel focusing the content.
    for (let l of this._listeners) {
      try {
        if ("shouldFocusContent" in l && !l.shouldFocusContent()) {
          return;
        }
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    this._browser.focus();
    this.sendMessageToContext("Finder:FocusContent");
  },

  onFindbarClose() {
    this._lastFoundBrowsingContext = null;
    this.sendMessageToAllContexts("Finder:FindbarClose");

    if (lazy.isLetterboxingEnabled) {
      let window = this._browser.ownerGlobal;
      if (window.RFPHelper) {
        window.RFPHelper.contentSizeUpdated(window);
      }
    }
  },

  onFindbarOpen() {
    this.sendMessageToAllContexts("Finder:FindbarOpen");

    if (lazy.isLetterboxingEnabled) {
      let window = this._browser.ownerGlobal;
      if (window.RFPHelper) {
        window.RFPHelper.contentSizeUpdated(window);
      }
    }
  },

  onModalHighlightChange(aUseModalHighlight) {
    this.sendMessageToAllContexts("Finder:ModalHighlightChange", {
      useModalHighlight: aUseModalHighlight,
    });
  },

  onHighlightAllChange(aHighlightAll) {
    this.sendMessageToAllContexts("Finder:HighlightAllChange", {
      highlightAll: aHighlightAll,
    });
  },

  keyPress(aEvent) {
    this.sendMessageToContext("Finder:KeyPress", {
      keyCode: aEvent.keyCode,
      ctrlKey: aEvent.ctrlKey,
      metaKey: aEvent.metaKey,
      altKey: aEvent.altKey,
      shiftKey: aEvent.shiftKey,
    });
  },

  initNotFoundSound() {
    if (!gSound && lazy.isSoundEnabled && lazy.notFoundSoundURL) {
      try {
        gSound = Cc["@mozilla.org/sound;1"].getService(Ci.nsISound);
        gSound.init();
      } catch (ex) {}
    }
  },

  playNotFoundSound() {
    if (!lazy.isSoundEnabled || !lazy.notFoundSoundURL) {
      return;
    }

    this.initNotFoundSound();
    if (!gSound) {
      return;
    }

    let soundUrl = lazy.notFoundSoundURL;
    if (soundUrl == "beep") {
      gSound.beep();
    } else {
      if (soundUrl == "default") {
        soundUrl = "chrome://global/content/notfound.wav";
      }
      gSound.play(Services.io.newURI(soundUrl));
    }
  },
};
