/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  const { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
  );

  class MozFindbar extends XULElement {
    constructor() {
      super();
      MozXULElement.insertFTLIfNeeded("toolkit/main-window/findbar.ftl");
      this.destroy = this.destroy.bind(this);

      // We have to guard against `this.close` being |null| due to an unknown
      // issue, which is tracked in bug 957999.
      this.addEventListener(
        "keypress",
        event => {
          if (event.keyCode == event.DOM_VK_ESCAPE) {
            if (this.close) {
              this.close();
            }
            event.preventDefault();
          }
        },
        true
      );

      this.content = MozXULElement.parseXULToFragment(`
      <hbox anonid="findbar-container" class="findbar-container" flex="1" align="center">
        <hbox anonid="findbar-textbox-wrapper" align="stretch">
          <html:input anonid="findbar-textbox" class="findbar-textbox findbar-find-fast" />
          <toolbarbutton anonid="find-previous" class="findbar-find-previous tabbable" data-l10n-attrs="tooltiptext" data-l10n-id="findbar-previous" oncommand="onFindAgainCommand(true);" disabled="true" />
          <toolbarbutton anonid="find-next" class="findbar-find-next tabbable" data-l10n-id="findbar-next" oncommand="onFindAgainCommand(false);" disabled="true" />
        </hbox>
        <toolbarbutton anonid="highlight" class="findbar-highlight findbar-button tabbable" data-l10n-id="findbar-highlight-all2" oncommand="toggleHighlight(this.checked);" type="checkbox" />
        <toolbarbutton anonid="find-case-sensitive" class="findbar-case-sensitive findbar-button tabbable" data-l10n-id="findbar-case-sensitive" oncommand="_setCaseSensitivity(this.checked ? 1 : 0);" type="checkbox" />
        <toolbarbutton anonid="find-entire-word" class="findbar-entire-word findbar-button tabbable" data-l10n-id="findbar-entire-word" oncommand="toggleEntireWord(this.checked);" type="checkbox" />
        <label anonid="match-case-status" class="findbar-find-fast" />
        <label anonid="entire-word-status" class="findbar-find-fast" />
        <label anonid="found-matches" class="findbar-find-fast found-matches" hidden="true" />
        <image anonid="find-status-icon" class="findbar-find-fast find-status-icon" />
        <description anonid="find-status" control="findbar-textbox" class="findbar-find-fast findbar-find-status" />
      </hbox>
      <toolbarbutton anonid="find-closebutton" class="findbar-closebutton close-icon" data-l10n-id="findbar-find-button-close" oncommand="close();" />
    `);
    }

    connectedCallback() {
      // Hide the findbar immediately without animation. This prevents a flicker in the case where
      // we'll never be shown (i.e. adopting a tab that has a previously-opened-but-now-closed
      // findbar into a new window).
      this.setAttribute("noanim", "true");
      this.hidden = true;
      this.appendChild(document.importNode(this.content, true));

      /**
       * Please keep in sync with toolkit/modules/FindBarChild.jsm
       */
      this.FIND_NORMAL = 0;

      this.FIND_TYPEAHEAD = 1;

      this.FIND_LINKS = 2;

      this.__findMode = 0;

      this._flashFindBar = 0;

      this._initialFlashFindBarCount = 6;

      /**
       * - For tests that need to know when the find bar is finished
       * - initializing, we store a promise to notify on.
       */
      this._startFindDeferred = null;

      this._browser = null;

      this.__prefsvc = null;

      this._observer = {
        _self: this,

        QueryInterface: ChromeUtils.generateQI([
          "nsIObserver",
          "nsISupportsWeakReference",
        ]),

        observe(aSubject, aTopic, aPrefName) {
          if (aTopic != "nsPref:changed") {
            return;
          }

          let prefsvc = this._self._prefsvc;

          switch (aPrefName) {
            case "accessibility.typeaheadfind":
              this._self._findAsYouType = prefsvc.getBoolPref(aPrefName);
              break;
            case "accessibility.typeaheadfind.manual":
              this._self._manualFAYT = prefsvc.getBoolPref(aPrefName);
              break;
            case "accessibility.typeaheadfind.timeout":
              this._self.quickFindTimeoutLength = prefsvc.getIntPref(aPrefName);
              break;
            case "accessibility.typeaheadfind.linksonly":
              this._self._typeAheadLinksOnly = prefsvc.getBoolPref(aPrefName);
              break;
            case "accessibility.typeaheadfind.casesensitive":
              this._self._setCaseSensitivity(prefsvc.getIntPref(aPrefName));
              break;
            case "findbar.entireword":
              this._self._entireWord = prefsvc.getBoolPref(aPrefName);
              this._self.toggleEntireWord(this._self._entireWord, true);
              break;
            case "findbar.highlightAll":
              this._self.toggleHighlight(prefsvc.getBoolPref(aPrefName), true);
              break;
            case "findbar.modalHighlight":
              this._self._useModalHighlight = prefsvc.getBoolPref(aPrefName);
              if (this._self.browser.finder) {
                this._self.browser.finder.onModalHighlightChange(
                  this._self._useModalHighlight
                );
              }
              break;
          }
        },
      };

      this._destroyed = false;

      this._pluralForm = null;

      this._strBundle = null;

      this._xulBrowserWindow = null;

      // These elements are accessed frequently and are therefore cached
      this._findField = this.getElement("findbar-textbox");
      this._foundMatches = this.getElement("found-matches");
      this._findStatusIcon = this.getElement("find-status-icon");
      this._findStatusDesc = this.getElement("find-status");

      this._foundURL = null;

      let prefsvc = this._prefsvc;

      this.quickFindTimeoutLength = prefsvc.getIntPref(
        "accessibility.typeaheadfind.timeout"
      );
      this._flashFindBar = prefsvc.getIntPref(
        "accessibility.typeaheadfind.flashBar"
      );
      this._useModalHighlight = prefsvc.getBoolPref("findbar.modalHighlight");

      prefsvc.addObserver("accessibility.typeaheadfind", this._observer);
      prefsvc.addObserver("accessibility.typeaheadfind.manual", this._observer);
      prefsvc.addObserver(
        "accessibility.typeaheadfind.linksonly",
        this._observer
      );
      prefsvc.addObserver(
        "accessibility.typeaheadfind.casesensitive",
        this._observer
      );
      prefsvc.addObserver("findbar.entireword", this._observer);
      prefsvc.addObserver("findbar.highlightAll", this._observer);
      prefsvc.addObserver("findbar.modalHighlight", this._observer);

      this._findAsYouType = prefsvc.getBoolPref("accessibility.typeaheadfind");
      this._manualFAYT = prefsvc.getBoolPref(
        "accessibility.typeaheadfind.manual"
      );
      this._typeAheadLinksOnly = prefsvc.getBoolPref(
        "accessibility.typeaheadfind.linksonly"
      );
      this._typeAheadCaseSensitive = prefsvc.getIntPref(
        "accessibility.typeaheadfind.casesensitive"
      );
      this._entireWord = prefsvc.getBoolPref("findbar.entireword");
      this._highlightAll = prefsvc.getBoolPref("findbar.highlightAll");

      // Convenience
      this.nsITypeAheadFind = Ci.nsITypeAheadFind;
      this.nsISelectionController = Ci.nsISelectionController;
      this._findSelection = this.nsISelectionController.SELECTION_FIND;

      this._findResetTimeout = -1;

      // Make sure the FAYT keypress listener is attached by initializing the
      // browser property
      if (this.getAttribute("browserid")) {
        setTimeout(
          function(aSelf) {
            // eslint-disable-next-line no-self-assign
            aSelf.browser = aSelf.browser;
          },
          0,
          this
        );
      }

      window.addEventListener("unload", this.destroy);

      this._findField.addEventListener("input", event => {
        // We should do nothing during composition.  E.g., composing string
        // before converting may matches a forward word of expected word.
        // After that, even if user converts the composition string to the
        // expected word, it may find second or later searching word in the
        // document.
        if (this._isIMEComposing) {
          return;
        }

        const value = this._findField.value;
        if (this._hadValue && !value) {
          this._willfullyDeleted = true;
          this._hadValue = false;
        } else if (value.trim()) {
          this._hadValue = true;
          this._willfullyDeleted = false;
        }
        this._find(value);
      });

      this._findField.addEventListener("keypress", event => {
        switch (event.keyCode) {
          case KeyEvent.DOM_VK_RETURN:
            if (this._findMode == this.FIND_NORMAL) {
              let findString = this._findField;
              if (!findString.value) {
                return;
              }
              if (event.getModifierState("Accel")) {
                this.getElement("highlight").click();
                return;
              }

              this.onFindAgainCommand(event.shiftKey);
            } else {
              this._finishFAYT(event);
            }
            break;
          case KeyEvent.DOM_VK_TAB:
            let shouldHandle =
              !event.altKey && !event.ctrlKey && !event.metaKey;
            if (shouldHandle && this._findMode != this.FIND_NORMAL) {
              this._finishFAYT(event);
            }
            break;
          case KeyEvent.DOM_VK_PAGE_UP:
          case KeyEvent.DOM_VK_PAGE_DOWN:
            if (
              !event.altKey &&
              !event.ctrlKey &&
              !event.metaKey &&
              !event.shiftKey
            ) {
              this.browser.finder.keyPress(event);
              event.preventDefault();
            }
            break;
          case KeyEvent.DOM_VK_UP:
          case KeyEvent.DOM_VK_DOWN:
            this.browser.finder.keyPress(event);
            event.preventDefault();
            break;
        }
      });

      this._findField.addEventListener("blur", event => {
        // Note: This code used to remove the selection
        // if it matched an editable.
        this.browser.finder.enableSelection();
      });

      this._findField.addEventListener("focus", event => {
        if (AppConstants.platform == "macosx") {
          this._onFindFieldFocus();
        }
        this._updateBrowserWithState();
      });

      this._findField.addEventListener("compositionstart", event => {
        // Don't close the find toolbar while IME is composing.
        let findbar = this;
        findbar._isIMEComposing = true;
        if (findbar._quickFindTimeout) {
          clearTimeout(findbar._quickFindTimeout);
          findbar._quickFindTimeout = null;
          findbar._updateBrowserWithState();
        }
      });

      this._findField.addEventListener("compositionend", event => {
        this._isIMEComposing = false;
        if (this._findMode != this.FIND_NORMAL) {
          this._setFindCloseTimeout();
        }
      });

      this._findField.addEventListener("dragover", event => {
        if (event.dataTransfer.types.includes("text/plain")) {
          event.preventDefault();
        }
      });

      this._findField.addEventListener("drop", event => {
        let value = event.dataTransfer.getData("text/plain");
        this._findField.value = value;
        this._find(value);
        event.stopPropagation();
        event.preventDefault();
      });
    }

    set _findMode(val) {
      this.__findMode = val;
      this._updateBrowserWithState();
      return val;
    }

    get _findMode() {
      return this.__findMode;
    }

    set prefillWithSelection(val) {
      this.setAttribute("prefillwithselection", val);
      return val;
    }

    get prefillWithSelection() {
      return this.getAttribute("prefillwithselection") != "false";
    }

    get findMode() {
      return this._findMode;
    }

    get hasTransactions() {
      if (this._findField.value) {
        return true;
      }

      // Watch out for lazy editor init
      if (this._findField.editor) {
        let tm = this._findField.editor.transactionManager;
        return !!(tm.numberOfUndoItems || tm.numberOfRedoItems);
      }
      return false;
    }

    set browser(val) {
      if (this._browser) {
        if (this._browser.messageManager) {
          this._browser.messageManager.removeMessageListener(
            "Findbar:Keypress",
            this
          );
          this._browser.messageManager.removeMessageListener(
            "Findbar:Mouseup",
            this
          );
        }
        let finder = this._browser.finder;
        if (finder) {
          finder.removeResultListener(this);
        }
      }

      this._browser = val;
      if (this._browser) {
        // Need to do this to ensure the correct initial state.
        this._updateBrowserWithState();
        this._browser.messageManager.addMessageListener(
          "Findbar:Keypress",
          this
        );
        this._browser.messageManager.addMessageListener(
          "Findbar:Mouseup",
          this
        );
        this._browser.finder.addResultListener(this);

        this._findField.value = this._browser._lastSearchString;
      }
      return val;
    }

    get browser() {
      if (!this._browser) {
        this._browser = document.getElementById(this.getAttribute("browserid"));
      }
      return this._browser;
    }

    get _prefsvc() {
      return Services.prefs;
    }

    get pluralForm() {
      if (!this._pluralForm) {
        this._pluralForm = ChromeUtils.import(
          "resource://gre/modules/PluralForm.jsm",
          {}
        ).PluralForm;
      }
      return this._pluralForm;
    }

    get strBundle() {
      if (!this._strBundle) {
        this._strBundle = Services.strings.createBundle(
          "chrome://global/locale/findbar.properties"
        );
      }
      return this._strBundle;
    }

    getElement(aAnonymousID) {
      return this.querySelector(`[anonid=${aAnonymousID}]`);
    }

    /**
     * This is necessary because the destructor isn't called when
     * we are removed from a document that is not destroyed. This
     * needs to be explicitly called in this case
     */
    destroy() {
      if (this._destroyed) {
        return;
      }
      window.removeEventListener("unload", this.destroy);
      this._destroyed = true;

      if (this.browser && this.browser.finder) {
        this.browser.finder.destroy();
      }

      this.browser = null;

      let prefsvc = this._prefsvc;
      prefsvc.removeObserver("accessibility.typeaheadfind", this._observer);
      prefsvc.removeObserver(
        "accessibility.typeaheadfind.manual",
        this._observer
      );
      prefsvc.removeObserver(
        "accessibility.typeaheadfind.linksonly",
        this._observer
      );
      prefsvc.removeObserver(
        "accessibility.typeaheadfind.casesensitive",
        this._observer
      );
      prefsvc.removeObserver("findbar.entireword", this._observer);
      prefsvc.removeObserver("findbar.highlightAll", this._observer);
      prefsvc.removeObserver("findbar.modalHighlight", this._observer);

      // Clear all timers that might still be running.
      this._cancelTimers();
    }

    _cancelTimers() {
      if (this._flashFindBarTimeout) {
        clearInterval(this._flashFindBarTimeout);
        this._flashFindBarTimeout = null;
      }
      if (this._quickFindTimeout) {
        clearTimeout(this._quickFindTimeout);
        this._quickFindTimeout = null;
      }
      if (this._findResetTimeout) {
        clearTimeout(this._findResetTimeout);
        this._findResetTimeout = null;
      }
    }

    _setFindCloseTimeout() {
      if (this._quickFindTimeout) {
        clearTimeout(this._quickFindTimeout);
      }

      // Don't close the find toolbar while IME is composing OR when the
      // findbar is already hidden.
      if (this._isIMEComposing || this.hidden) {
        this._quickFindTimeout = null;
        this._updateBrowserWithState();
        return;
      }

      if (this.quickFindTimeoutLength < 1) {
        this._quickFindTimeout = null;
      } else {
        this._quickFindTimeout = setTimeout(() => {
          if (this._findMode != this.FIND_NORMAL) {
            this.close();
          }
          this._quickFindTimeout = null;
        }, this.quickFindTimeoutLength);
      }
      this._updateBrowserWithState();
    }

    /**
     * - Updates the search match count after each find operation on a new string.
     * - @param aRes
     * -        the result of the find operation
     */
    _updateMatchesCount() {
      if (!this._dispatchFindEvent("matchescount")) {
        return;
      }

      this.browser.finder.requestMatchesCount(
        this._findField.value,
        this._findMode == this.FIND_LINKS
      );
    }

    /**
     * - Turns highlight on or off.
     * - @param aHighlight (boolean)
     * -        Whether to turn the highlight on or off
     * - @param aFromPrefObserver (boolean)
     * -        Whether the callee is the pref observer, which means we should
     * -        not set the same pref again.
     */
    toggleHighlight(aHighlight, aFromPrefObserver) {
      if (aHighlight === this._highlightAll) {
        return;
      }

      this.browser.finder.onHighlightAllChange(aHighlight);

      this._setHighlightAll(aHighlight, aFromPrefObserver);

      if (!this._dispatchFindEvent("highlightallchange")) {
        return;
      }

      let word = this._findField.value;
      // Bug 429723. Don't attempt to highlight ""
      if (aHighlight && !word) {
        return;
      }

      this.browser.finder.highlight(
        aHighlight,
        word,
        this._findMode == this.FIND_LINKS
      );

      // Update the matches count
      this._updateMatchesCount(this.nsITypeAheadFind.FIND_FOUND);
    }

    /**
     * - Updates the highlight-all mode of the findbar and its UI.
     * - @param aHighlight (boolean)
     * -        Whether to turn the highlight on or off.
     * - @param aFromPrefObserver (boolean)
     * -        Whether the callee is the pref observer, which means we should
     * -        not set the same pref again.
     */
    _setHighlightAll(aHighlight, aFromPrefObserver) {
      if (typeof aHighlight != "boolean") {
        aHighlight = this._highlightAll;
      }
      if (aHighlight !== this._highlightAll) {
        this._highlightAll = aHighlight;
        if (!aFromPrefObserver) {
          this._prefsvc.setBoolPref("findbar.highlightAll", aHighlight);
        }
      }
      let checkbox = this.getElement("highlight");
      checkbox.checked = this._highlightAll;
    }

    /**
     * - Updates the case-sensitivity mode of the findbar and its UI.
     * - @param [optional] aString
     * -        The string for which case sensitivity might be turned on.
     * -        This only used when case-sensitivity is in auto mode,
     * -        @see _shouldBeCaseSensitive. The default value for this
     * -        parameter is the find-field value.
     */
    _updateCaseSensitivity(aString) {
      let val = aString || this._findField.value;

      let caseSensitive = this._shouldBeCaseSensitive(val);
      let checkbox = this.getElement("find-case-sensitive");
      let statusLabel = this.getElement("match-case-status");
      checkbox.checked = caseSensitive;

      statusLabel.value = caseSensitive ? this._caseSensitiveStr : "";

      // Show the checkbox on the full Find bar in non-auto mode.
      // Show the label in all other cases.
      let hideCheckbox =
        this._findMode != this.FIND_NORMAL ||
        (this._typeAheadCaseSensitive != 0 &&
          this._typeAheadCaseSensitive != 1);
      checkbox.hidden = hideCheckbox;
      statusLabel.hidden = !hideCheckbox;

      this.browser.finder.caseSensitive = caseSensitive;
    }

    /**
     * - Sets the findbar case-sensitivity mode
     * - @param aCaseSensitivity (int)
     * -   0 - case insensitive
     * -   1 - case sensitive
     * -   2 - auto = case sensitive iff match string contains upper case letters
     * -   @see _shouldBeCaseSensitive
     */
    _setCaseSensitivity(aCaseSensitivity) {
      this._typeAheadCaseSensitive = aCaseSensitivity;
      this._updateCaseSensitivity();
      this._findFailedString = null;
      this._find();

      this._dispatchFindEvent("casesensitivitychange");
    }

    /**
     * - Updates the entire-word mode of the findbar and its UI.
     */
    _setEntireWord() {
      let entireWord = this._entireWord;
      let checkbox = this.getElement("find-entire-word");
      let statusLabel = this.getElement("entire-word-status");
      checkbox.checked = entireWord;

      statusLabel.value = entireWord ? this._entireWordStr : "";

      // Show the checkbox on the full Find bar in non-auto mode.
      // Show the label in all other cases.
      let hideCheckbox = this._findMode != this.FIND_NORMAL;
      checkbox.hidden = hideCheckbox;
      statusLabel.hidden = !hideCheckbox;

      this.browser.finder.entireWord = entireWord;
    }

    /**
     * - Sets the findbar entire-word mode
     * - @param aEntireWord (boolean)
     * - Whether or not entire-word mode should be turned on.
     */
    toggleEntireWord(aEntireWord, aFromPrefObserver) {
      if (!aFromPrefObserver) {
        // Just set the pref; our observer will change the find bar behavior.
        this._prefsvc.setBoolPref("findbar.entireword", aEntireWord);
        return;
      }

      this._findFailedString = null;
      this._find();
    }

    /**
     * - Opens and displays the find bar.
     * -
     * - @param aMode
     * -        the find mode to be used, which is either FIND_NORMAL,
     * -        FIND_TYPEAHEAD or FIND_LINKS. If not passed, the last
     * -        find mode if any or FIND_NORMAL.
     * - @returns true if the find bar wasn't previously open, false otherwise.
     */
    open(aMode) {
      if (aMode != undefined) {
        this._findMode = aMode;
      }

      if (!this._notFoundStr) {
        var stringsBundle = this.strBundle;
        this._notFoundStr = stringsBundle.GetStringFromName("NotFound");
        this._wrappedToTopStr = stringsBundle.GetStringFromName("WrappedToTop");
        this._wrappedToBottomStr = stringsBundle.GetStringFromName(
          "WrappedToBottom"
        );
        this._normalFindStr = stringsBundle.GetStringFromName("NormalFind");
        this._fastFindStr = stringsBundle.GetStringFromName("FastFind");
        this._fastFindLinksStr = stringsBundle.GetStringFromName(
          "FastFindLinks"
        );
        this._caseSensitiveStr = stringsBundle.GetStringFromName(
          "CaseSensitive"
        );
        this._entireWordStr = stringsBundle.GetStringFromName("EntireWord");
      }

      this._findFailedString = null;

      this._updateFindUI();
      if (this.hidden) {
        this.removeAttribute("noanim");
        this.hidden = false;

        this._updateStatusUI(this.nsITypeAheadFind.FIND_FOUND);

        let event = document.createEvent("Events");
        event.initEvent("findbaropen", true, false);
        this.dispatchEvent(event);

        this.browser.finder.onFindbarOpen();

        return true;
      }
      return false;
    }

    /**
     * - Closes the findbar.
     */
    close(aNoAnim) {
      if (this.hidden) {
        return;
      }

      if (aNoAnim) {
        this.setAttribute("noanim", true);
      }
      this.hidden = true;

      let event = document.createEvent("Events");
      event.initEvent("findbarclose", true, false);
      this.dispatchEvent(event);

      // 'focusContent()' iterates over all listeners in the chrome
      // process, so we need to call it from here.
      this.browser.finder.focusContent();
      this.browser.finder.onFindbarClose();

      this._cancelTimers();
      this._updateBrowserWithState();

      this._findFailedString = null;
    }

    clear() {
      this.browser.finder.removeSelection();
      // Clear value and undo/redo transactions
      this._findField.value = "";
      if (this._findField.editor) {
        this._findField.editor.transactionManager.clear();
      }
      this.toggleHighlight(false);
      this._updateStatusUI();
      this._enableFindButtons(false);
    }

    _dispatchKeypressEvent(aTarget, fakeEvent) {
      if (!aTarget) {
        return;
      }

      // The event information comes from the child process. If we need more
      // properties/information here, change the list of sent properties in
      // browser-content.js
      let event = new aTarget.ownerGlobal.KeyboardEvent(
        fakeEvent.type,
        fakeEvent
      );
      aTarget.dispatchEvent(event);
    }

    _updateStatusUIBar(aFoundURL) {
      if (!this._xulBrowserWindow) {
        try {
          this._xulBrowserWindow = window.docShell.treeOwner
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIXULWindow).XULBrowserWindow;
        } catch (ex) {}
        if (!this._xulBrowserWindow) {
          return false;
        }
      }

      // Call this has the same effect like hovering over link,
      // the browser shows the URL as a tooltip.
      this._xulBrowserWindow.setOverLink(aFoundURL || "", null);
      return true;
    }

    _finishFAYT(aKeypressEvent) {
      this.browser.finder.focusContent();

      if (aKeypressEvent) {
        aKeypressEvent.preventDefault();
      }

      this.browser.finder.keyPress(aKeypressEvent);

      this.close();
      return true;
    }

    _shouldBeCaseSensitive(aString) {
      if (this._typeAheadCaseSensitive == 0) {
        return false;
      }
      if (this._typeAheadCaseSensitive == 1) {
        return true;
      }

      return aString != aString.toLowerCase();
    }

    /**
     * We get a fake event object through an IPC message when FAYT is being used
     * from within the browser. We then stuff that input in the find bar here.
     */
    _onBrowserKeypress(aFakeEvent) {
      const FAYT_LINKS_KEY = "'";
      const FAYT_TEXT_KEY = "/";

      if (!this.hidden && this._findField == document.activeElement) {
        this._dispatchKeypressEvent(this._findField, aFakeEvent);
        return;
      }

      if (this._findMode != this.FIND_NORMAL && this._quickFindTimeout) {
        this._findField.select();
        this._findField.focus();
        this._dispatchKeypressEvent(this._findField, aFakeEvent);
        return;
      }

      let key = aFakeEvent.charCode
        ? String.fromCharCode(aFakeEvent.charCode)
        : null;
      let manualstartFAYT =
        (key == FAYT_LINKS_KEY || key == FAYT_TEXT_KEY) && this._manualFAYT;
      let autostartFAYT =
        !manualstartFAYT && this._findAsYouType && key && key != " ";
      if (manualstartFAYT || autostartFAYT) {
        let mode =
          key == FAYT_LINKS_KEY || (autostartFAYT && this._typeAheadLinksOnly)
            ? this.FIND_LINKS
            : this.FIND_TYPEAHEAD;

        // Clear bar first, so that when openFindBar() calls setCaseSensitivity()
        // it doesn't get confused by a lingering value
        this._findField.value = "";

        this.open(mode);
        this._setFindCloseTimeout();
        this._findField.select();
        this._findField.focus();

        if (autostartFAYT) {
          this._dispatchKeypressEvent(this._findField, aFakeEvent);
        } else {
          this._updateStatusUI(this.nsITypeAheadFind.FIND_FOUND);
        }
      }
    }

    /**
     * See MessageListener
     */
    receiveMessage(aMessage) {
      if (aMessage.target != this._browser) {
        return undefined;
      }
      switch (aMessage.name) {
        case "Findbar:Mouseup":
          if (!this.hidden && this._findMode != this.FIND_NORMAL) {
            this.close();
          }
          break;
        case "Findbar:Keypress":
          this._onBrowserKeypress(aMessage.data);
          break;
      }
      return undefined;
    }

    _updateBrowserWithState() {
      if (this._browser && this._browser.messageManager) {
        this._browser.messageManager.sendAsyncMessage("Findbar:UpdateState", {
          findMode: this._findMode,
          isOpenAndFocused:
            !this.hidden && document.activeElement == this._findField,
          hasQuickFindTimeout: !!this._quickFindTimeout,
        });
      }
    }

    _enableFindButtons(aEnable) {
      this.getElement("find-next").disabled = this.getElement(
        "find-previous"
      ).disabled = !aEnable;
    }

    /**
     * - Determines whether minimalist or general-purpose search UI is to be
     * - displayed when the find bar is activated.
     */
    _updateFindUI() {
      let showMinimalUI = this._findMode != this.FIND_NORMAL;

      let nodes = this.getElement("findbar-container").children;
      let wrapper = this.getElement("findbar-textbox-wrapper");
      let foundMatches = this._foundMatches;
      for (let node of nodes) {
        if (node == wrapper || node == foundMatches) {
          continue;
        }
        node.hidden = showMinimalUI;
      }
      this.getElement("find-next").hidden = this.getElement(
        "find-previous"
      ).hidden = showMinimalUI;
      foundMatches.hidden = showMinimalUI || !foundMatches.value;
      this._updateCaseSensitivity();
      this._setEntireWord();
      this._setHighlightAll();

      if (showMinimalUI) {
        this._findField.classList.add("minimal");
      } else {
        this._findField.classList.remove("minimal");
      }

      if (this._findMode == this.FIND_TYPEAHEAD) {
        this._findField.placeholder = this._fastFindStr;
      } else if (this._findMode == this.FIND_LINKS) {
        this._findField.placeholder = this._fastFindLinksStr;
      } else {
        this._findField.placeholder = this._normalFindStr;
      }
    }

    _find(aValue) {
      if (!this._dispatchFindEvent("")) {
        return;
      }

      let val = aValue || this._findField.value;

      // We have to carry around an explicit version of this,
      // because finder.searchString doesn't update on failed
      // searches.
      this.browser._lastSearchString = val;

      // Only search on input if we don't have a last-failed string,
      // or if the current search string doesn't start with it.
      // In entire-word mode we always attemp a find; since sequential matching
      // is not guaranteed, the first character typed may not be a word (no
      // match), but the with the second character it may well be a word,
      // thus a match.
      if (
        !this._findFailedString ||
        !val.startsWith(this._findFailedString) ||
        this._entireWord
      ) {
        // Getting here means the user commanded a find op. Make sure any
        // initial prefilling is ignored if it hasn't happened yet.
        if (this._startFindDeferred) {
          this._startFindDeferred.resolve();
          this._startFindDeferred = null;
        }

        this._enableFindButtons(val);
        this._updateCaseSensitivity(val);
        this._setEntireWord();

        this.browser.finder.fastFind(
          val,
          this._findMode == this.FIND_LINKS,
          this._findMode != this.FIND_NORMAL
        );
      }

      if (this._findMode != this.FIND_NORMAL) {
        this._setFindCloseTimeout();
      }

      if (this._findResetTimeout != -1) {
        clearTimeout(this._findResetTimeout);
      }

      // allow a search to happen on input again after a second has
      // expired since the previous input, to allow for dynamic
      // content and/or page loading
      this._findResetTimeout = setTimeout(() => {
        this._findFailedString = null;
        this._findResetTimeout = -1;
      }, 1000);
    }

    _flash() {
      if (this._flashFindBarCount === undefined) {
        this._flashFindBarCount = this._initialFlashFindBarCount;
      }

      if (this._flashFindBarCount-- == 0) {
        clearInterval(this._flashFindBarTimeout);
        this._findField.removeAttribute("flash");
        this._flashFindBarCount = 6;
        return;
      }

      this._findField.setAttribute(
        "flash",
        this._flashFindBarCount % 2 == 0 ? "false" : "true"
      );
    }

    _findAgain(aFindPrevious) {
      this.browser.finder.findAgain(
        this._findField.value,
        aFindPrevious,
        this._findMode == this.FIND_LINKS,
        this._findMode != this.FIND_NORMAL
      );
    }

    _updateStatusUI(res, aFindPrevious) {
      switch (res) {
        case this.nsITypeAheadFind.FIND_WRAPPED:
          this._findStatusIcon.setAttribute("status", "wrapped");
          this._findStatusDesc.textContent = aFindPrevious
            ? this._wrappedToBottomStr
            : this._wrappedToTopStr;
          this._findField.removeAttribute("status");
          break;
        case this.nsITypeAheadFind.FIND_NOTFOUND:
          this._findStatusIcon.setAttribute("status", "notfound");
          this._findStatusDesc.textContent = this._notFoundStr;
          this._findField.setAttribute("status", "notfound");
          break;
        case this.nsITypeAheadFind.FIND_PENDING:
          this._findStatusIcon.setAttribute("status", "pending");
          this._findStatusDesc.textContent = "";
          this._findField.removeAttribute("status");
          break;
        case this.nsITypeAheadFind.FIND_FOUND:
        default:
          this._findStatusIcon.removeAttribute("status");
          this._findStatusDesc.textContent = "";
          this._findField.removeAttribute("status");
          break;
      }
    }

    updateControlState(aResult, aFindPrevious) {
      this._updateStatusUI(aResult, aFindPrevious);
      this._enableFindButtons(
        aResult !== this.nsITypeAheadFind.FIND_NOTFOUND &&
          !!this._findField.value
      );
    }

    _dispatchFindEvent(aType, aFindPrevious) {
      let event = document.createEvent("CustomEvent");
      event.initCustomEvent("find" + aType, true, true, {
        query: this._findField.value,
        caseSensitive: !!this._typeAheadCaseSensitive,
        entireWord: this._entireWord,
        highlightAll: this._highlightAll,
        findPrevious: aFindPrevious,
      });
      return this.dispatchEvent(event);
    }

    /**
     * - Opens the findbar, focuses the findfield and selects its contents.
     * - Also flashes the findbar the first time it's used.
     * - @param aMode
     * -        the find mode to be used, which is either FIND_NORMAL,
     * -        FIND_TYPEAHEAD or FIND_LINKS. If not passed, the last
     * -        find mode if any or FIND_NORMAL.
     */
    startFind(aMode) {
      let prefsvc = this._prefsvc;
      let userWantsPrefill = true;
      this.open(aMode);

      if (this._flashFindBar) {
        this._flashFindBarTimeout = setInterval(() => this._flash(), 500);
        prefsvc.setIntPref(
          "accessibility.typeaheadfind.flashBar",
          --this._flashFindBar
        );
      }

      let { PromiseUtils } = ChromeUtils.import(
        "resource://gre/modules/PromiseUtils.jsm"
      );
      this._startFindDeferred = PromiseUtils.defer();
      let startFindPromise = this._startFindDeferred.promise;

      if (this.prefillWithSelection) {
        userWantsPrefill = prefsvc.getBoolPref(
          "accessibility.typeaheadfind.prefillwithselection"
        );
      }

      if (this.prefillWithSelection && userWantsPrefill) {
        // NB: We have to focus this._findField here so tests that send
        // key events can open and close the find bar synchronously.
        this._findField.focus();

        // (e10s) since we focus lets also select it, otherwise that would
        // only happen in this.onCurrentSelection and, because it is async,
        // there's a chance keypresses could come inbetween, leading to
        // jumbled up queries.
        this._findField.select();

        this.browser.finder.getInitialSelection();
        return startFindPromise;
      }

      // If userWantsPrefill is false but prefillWithSelection is true,
      // then we might need to check the selection clipboard. Call
      // onCurrentSelection to do so.
      // Note: this.onCurrentSelection clears this._startFindDeferred.
      this.onCurrentSelection("", true);
      return startFindPromise;
    }

    /**
     * - Convenient alias to startFind(gFindBar.FIND_NORMAL);
     * -
     * - You should generally map the window's find command to this method.
     * -   e.g. <command name="cmd_find" oncommand="gFindBar.onFindCommand();"/>
     */
    onFindCommand() {
      return this.startFind(this.FIND_NORMAL);
    }

    /**
     * - Stub for find-next and find-previous commands
     * - @param aFindPrevious
     * -        true for find-previous, false otherwise.
     */
    onFindAgainCommand(aFindPrevious) {
      let findString =
        this._browser.finder.searchString || this._findField.value;
      if (!findString) {
        return this.startFind();
      }

      // We dispatch the findAgain event here instead of in _findAgain since
      // if there is a find event handler that prevents the default then
      // finder.searchString will never get updated which in turn means
      // there would never be findAgain events because of the logic below.
      if (!this._dispatchFindEvent("again", aFindPrevious)) {
        return undefined;
      }

      // user explicitly requested another search, so do it even if we think it'll fail
      this._findFailedString = null;

      // Ensure the stored SearchString is in sync with what we want to find
      if (this._findField.value != this._browser.finder.searchString) {
        this._find(this._findField.value);
      } else {
        this._findAgain(aFindPrevious);
        if (this._useModalHighlight) {
          this.open();
          this._findField.focus();
        }
      }

      return undefined;
    }

    /* Fetches the currently selected text and sets that as the text to search
     next. This is a MacOS specific feature. */
    onFindSelectionCommand() {
      this.browser.finder.setSearchStringToSelection().then(searchInfo => {
        if (searchInfo.selectedText) {
          this._findField.value = searchInfo.selectedText;
        }
      });
    }

    _onFindFieldFocus() {
      let prefsvc = this._prefsvc;
      const kPref = "accessibility.typeaheadfind.prefillwithselection";
      if (this.prefillWithSelection && prefsvc.getBoolPref(kPref)) {
        return;
      }

      let clipboardSearchString = this._browser.finder.clipboardSearchString;
      if (
        clipboardSearchString &&
        this._findField.value != clipboardSearchString &&
        !this._findField._willfullyDeleted
      ) {
        this._findField.value = clipboardSearchString;
        this._findField._hadValue = true;
        // Changing the search string makes the previous status invalid, so
        // we better clear it here.
        this._updateStatusUI();
      }
    }

    /**
     * - This handles all the result changes for both
     * - type-ahead-find and highlighting.
     * - @param aResult
     * -   One of the nsITypeAheadFind.FIND_* constants
     * -   indicating the result of a search operation.
     * - @param aFindBackwards
     * -   If the search was done from the bottom to
     * -   the top. This is used for right error messages
     * -   when reaching "the end of the page".
     * - @param aLinkURL
     * -   When a link matched then its URK. Always null
     * -   when not in FIND_LINKS mode.
     */
    onFindResult(aData) {
      if (aData.result == this.nsITypeAheadFind.FIND_NOTFOUND) {
        // If an explicit Find Again command fails, re-open the toolbar.
        if (aData.storeResult && this.open()) {
          this._findField.select();
          this._findField.focus();
        }
        this._findFailedString = aData.searchString;
      } else {
        this._findFailedString = null;
      }

      this._updateStatusUI(aData.result, aData.findBackwards);
      this._updateStatusUIBar(aData.linkURL);

      if (this._findMode != this.FIND_NORMAL) {
        this._setFindCloseTimeout();
      }
    }

    /**
     * - This handles all the result changes for matches counts.
     * - @param aResult
     * -   Result Object, containing the total amount of matches and a vector
     * -   of the current result.
     */
    onMatchesCountResult(aResult) {
      if (aResult.total !== 0) {
        if (aResult.total == -1) {
          this._foundMatches.value = this.pluralForm
            .get(
              aResult.limit,
              this.strBundle.GetStringFromName("FoundMatchesCountLimit")
            )
            .replace("#1", aResult.limit);
        } else {
          this._foundMatches.value = this.pluralForm
            .get(
              aResult.total,
              this.strBundle.GetStringFromName("FoundMatches")
            )
            .replace("#1", aResult.current)
            .replace("#2", aResult.total);
        }
        this._foundMatches.hidden = false;
      } else {
        this._foundMatches.hidden = true;
        this._foundMatches.value = "";
      }
    }

    onHighlightFinished(result) {
      // Noop.
    }

    onCurrentSelection(aSelectionString, aIsInitialSelection) {
      // Ignore the prefill if the user has already typed in the findbar,
      // it would have been overwritten anyway. See bug 1198465.
      if (aIsInitialSelection && !this._startFindDeferred) {
        return;
      }

      if (
        AppConstants.platform == "macosx" &&
        aIsInitialSelection &&
        !aSelectionString
      ) {
        let clipboardSearchString = this.browser.finder.clipboardSearchString;
        if (clipboardSearchString) {
          aSelectionString = clipboardSearchString;
        }
      }

      if (aSelectionString) {
        this._findField.value = aSelectionString;
      }

      if (aIsInitialSelection) {
        this._enableFindButtons(!!this._findField.value);
        this._findField.select();
        this._findField.focus();

        this._startFindDeferred.resolve();
        this._startFindDeferred = null;
      }
    }

    /**
     * - This handler may cancel a request to focus content by returning |false|
     * - explicitly.
     */
    shouldFocusContent() {
      const fm = Services.focus;
      if (fm.focusedWindow != window) {
        return false;
      }

      let focusedElement = fm.focusedElement;
      if (!focusedElement) {
        return false;
      }

      let focusedParent = focusedElement.closest("findbar");
      if (focusedParent != this && focusedParent != this._findField) {
        return false;
      }

      return true;
    }

    disconnectedCallback() {
      // Empty the DOM. We will rebuild if reconnected.
      while (this.lastChild) {
        this.removeChild(this.lastChild);
      }
      this.destroy();
    }
  }

  customElements.define("findbar", MozFindbar);
}
