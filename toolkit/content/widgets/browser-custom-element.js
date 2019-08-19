/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  const { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
  );

  let LazyModules = {};

  ChromeUtils.defineModuleGetter(
    LazyModules,
    "PermitUnloader",
    "resource://gre/actors/BrowserElementParent.jsm"
  );

  const elementsToDestroyOnUnload = new Set();

  window.addEventListener(
    "unload",
    () => {
      for (let element of elementsToDestroyOnUnload.values()) {
        element.destroy();
      }
      elementsToDestroyOnUnload.clear();
    },
    { mozSystemGroup: true, once: true }
  );

  class MozBrowser extends MozElements.MozElementMixin(XULFrameElement) {
    static get observedAttributes() {
      return ["remote"];
    }

    attributeChangedCallback(name, oldValue, newValue) {
      // When we have already been set up via connectedCallback and the
      // and the [remote] value changes, we need to start over. This used
      // to happen due to a XBL binding change.
      //
      // Only do this when the rebuild frameloaders pref is off. This update isn't
      // required when we rebuild the frameloaders in the backend.
      if (
        !Services.prefs.getBoolPref(
          "fission.rebuild_frameloaders_on_remoteness_change",
          false
        ) &&
        name === "remote" &&
        oldValue != newValue &&
        this.isConnectedAndReady
      ) {
        this.destroy();
        this.construct();
      }
    }

    constructor() {
      super();

      this.onPageHide = this.onPageHide.bind(this);

      this.isNavigating = false;

      this._documentURI = null;
      this._characterSet = null;
      this._documentContentType = null;

      /**
       * These are managed by the tabbrowser:
       */
      this.droppedLinkHandler = null;
      this.mIconURL = null;
      this.lastURI = null;

      this.addEventListener(
        "keypress",
        event => {
          if (event.keyCode != KeyEvent.DOM_VK_F7) {
            return;
          }

          if (event.defaultPrevented || !event.isTrusted) {
            return;
          }

          const kPrefShortcutEnabled =
            "accessibility.browsewithcaret_shortcut.enabled";
          const kPrefWarnOnEnable = "accessibility.warn_on_browsewithcaret";
          const kPrefCaretBrowsingOn = "accessibility.browsewithcaret";

          var isEnabled = this.mPrefs.getBoolPref(kPrefShortcutEnabled);
          if (!isEnabled) {
            return;
          }

          // Toggle browse with caret mode
          var browseWithCaretOn = this.mPrefs.getBoolPref(
            kPrefCaretBrowsingOn,
            false
          );
          var warn = this.mPrefs.getBoolPref(kPrefWarnOnEnable, true);
          if (warn && !browseWithCaretOn) {
            var checkValue = { value: false };
            var promptService = Services.prompt;

            var buttonPressed = promptService.confirmEx(
              window,
              this.mStrBundle.GetStringFromName(
                "browsewithcaret.checkWindowTitle"
              ),
              this.mStrBundle.GetStringFromName("browsewithcaret.checkLabel"),
              // Make "No" the default:
              promptService.STD_YES_NO_BUTTONS |
                promptService.BUTTON_POS_1_DEFAULT,
              null,
              null,
              null,
              this.mStrBundle.GetStringFromName("browsewithcaret.checkMsg"),
              checkValue
            );
            if (buttonPressed != 0) {
              if (checkValue.value) {
                try {
                  this.mPrefs.setBoolPref(kPrefShortcutEnabled, false);
                } catch (ex) {}
              }
              return;
            }
            if (checkValue.value) {
              try {
                this.mPrefs.setBoolPref(kPrefWarnOnEnable, false);
              } catch (ex) {}
            }
          }

          // Toggle the pref
          try {
            this.mPrefs.setBoolPref(kPrefCaretBrowsingOn, !browseWithCaretOn);
          } catch (ex) {}
        },
        { mozSystemGroup: true }
      );

      this.addEventListener(
        "dragover",
        event => {
          if (!this.droppedLinkHandler || event.defaultPrevented) {
            return;
          }

          // For drags that appear to be internal text (for example, tab drags),
          // set the dropEffect to 'none'. This prevents the drop even if some
          // other listener cancelled the event.
          var types = event.dataTransfer.types;
          if (
            types.includes("text/x-moz-text-internal") &&
            !types.includes("text/plain")
          ) {
            event.dataTransfer.dropEffect = "none";
            event.stopPropagation();
            event.preventDefault();
          }

          // No need to handle "dragover" in e10s, since nsDocShellTreeOwner.cpp in the child process
          // handles that case using "@mozilla.org/content/dropped-link-handler;1" service.
          if (this.isRemoteBrowser) {
            return;
          }

          let linkHandler = Services.droppedLinkHandler;
          if (linkHandler.canDropLink(event, false)) {
            event.preventDefault();
          }
        },
        { mozSystemGroup: true }
      );

      this.addEventListener(
        "drop",
        event => {
          // No need to handle "drop" in e10s, since nsDocShellTreeOwner.cpp in the child process
          // handles that case using "@mozilla.org/content/dropped-link-handler;1" service.
          if (
            !this.droppedLinkHandler ||
            event.defaultPrevented ||
            this.isRemoteBrowser
          ) {
            return;
          }

          let linkHandler = Services.droppedLinkHandler;
          try {
            // Pass true to prevent the dropping of javascript:/data: URIs
            var links = linkHandler.dropLinks(event, true);
          } catch (ex) {
            return;
          }

          if (links.length) {
            let triggeringPrincipal = linkHandler.getTriggeringPrincipal(event);
            this.droppedLinkHandler(event, links, triggeringPrincipal);
          }
        },
        { mozSystemGroup: true }
      );

      this.addEventListener("dragstart", event => {
        // If we're a remote browser dealing with a dragstart, stop it
        // from propagating up, since our content process should be dealing
        // with the mouse movement.
        if (this.isRemoteBrowser) {
          event.stopPropagation();
        }
      });
    }

    resetFields() {
      if (this.observer) {
        try {
          Services.obs.removeObserver(
            this.observer,
            "browser:purge-session-history"
          );
        } catch (ex) {
          // It's not clear why this sometimes throws an exception.
        }
        this.observer = null;
      }

      let browser = this;
      this.observer = {
        observe(aSubject, aTopic, aState) {
          if (aTopic == "browser:purge-session-history") {
            browser.purgeSessionHistory();
          } else if (aTopic == "apz:cancel-autoscroll") {
            if (aState == browser._autoScrollScrollId) {
              // Set this._autoScrollScrollId to null, so in stopScroll() we
              // don't call stopApzAutoscroll() (since it's APZ that
              // initiated the stopping).
              browser._autoScrollScrollId = null;
              browser._autoScrollPresShellId = null;

              browser._autoScrollPopup.hidePopup();
            }
          }
        },
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIObserver,
          Ci.nsISupportsWeakReference,
        ]),
      };

      this._documentURI = null;

      this._documentContentType = null;

      /**
       * Weak reference to an optional frame loader that can be used to influence
       * process selection for this browser.
       * See nsIBrowser.sameProcessAsFrameLoader.
       */
      this._sameProcessAsFrameLoader = null;

      this._loadContext = null;

      this._imageDocument = null;

      this._webBrowserFind = null;

      this._finder = null;

      this._remoteFinder = null;

      this._fastFind = null;

      this._outerWindowID = null;

      this._innerWindowID = null;

      this._lastSearchString = null;

      this._controller = null;

      this._remoteWebNavigation = null;

      this._remoteWebProgress = null;

      this._contentTitle = "";

      this._characterSet = "";

      this._mayEnableCharacterEncodingMenu = null;

      this._charsetAutodetected = false;

      this._contentPrincipal = null;

      this._contentStoragePrincipal = null;

      this._contentBlockingAllowListPrincipal = null;

      this._csp = null;

      this._referrerInfo = null;

      this._contentRequestContextID = null;

      this._fullZoom = 1;

      this._textZoom = 1;

      this._isSyntheticDocument = false;

      this.mPrefs = Services.prefs;

      this._mStrBundle = null;

      this.blockedPopups = null;

      this._audioMuted = false;

      this._hasAnyPlayingMediaBeenBlocked = false;

      /**
       * Only send the message "Browser:UnselectedTabHover" when someone requests
       * for the message, which can reduce non-necessary communication.
       */
      this._shouldSendUnselectedTabHover = false;

      this._unselectedTabHoverMessageListenerCount = 0;

      this._securityUI = null;

      this.urlbarChangeTracker = {
        _startedLoadSinceLastUserTyping: false,

        startedLoad() {
          this._startedLoadSinceLastUserTyping = true;
        },
        finishedLoad() {
          this._startedLoadSinceLastUserTyping = false;
        },
        userTyped() {
          this._startedLoadSinceLastUserTyping = false;
        },
      };

      this._userTypedValue = null;

      this._AUTOSCROLL_SNAP = 10;

      this._scrolling = false;

      this._startX = null;

      this._startY = null;

      this._autoScrollPopup = null;

      this._autoScrollNeedsCleanup = false;

      /**
       * These IDs identify the scroll frame being autoscrolled.
       */
      this._autoScrollScrollId = null;

      this._autoScrollPresShellId = null;
    }

    connectedCallback() {
      // We typically use this to avoid running JS that triggers a layout during parse
      // (see comment on the delayConnectedCallback implementation). In this case, we
      // are using it to avoid a leak - see https://bugzilla.mozilla.org/show_bug.cgi?id=1441935#c20.
      if (this.delayConnectedCallback()) {
        return;
      }

      this.construct();
    }

    disconnectedCallback() {
      this.destroy();
    }

    get autoscrollEnabled() {
      if (this.getAttribute("autoscroll") == "false") {
        return false;
      }

      return this.mPrefs.getBoolPref("general.autoScroll", true);
    }

    get canGoBack() {
      return this.webNavigation.canGoBack;
    }

    get canGoForward() {
      return this.webNavigation.canGoForward;
    }

    get currentURI() {
      if (this.webNavigation) {
        return this.webNavigation.currentURI;
      }
      return null;
    }

    get documentURI() {
      return this.isRemoteBrowser
        ? this._documentURI
        : this.contentDocument.documentURIObject;
    }

    get documentContentType() {
      if (this.isRemoteBrowser) {
        return this._documentContentType;
      }
      return this.contentDocument ? this.contentDocument.contentType : null;
    }

    set documentContentType(aContentType) {
      if (aContentType != null) {
        if (this.isRemoteBrowser) {
          this._documentContentType = aContentType;
        } else {
          this.contentDocument.documentContentType = aContentType;
        }
      }
    }

    set sameProcessAsFrameLoader(val) {
      this._sameProcessAsFrameLoader = Cu.getWeakReference(val);
    }

    get sameProcessAsFrameLoader() {
      return (
        this._sameProcessAsFrameLoader && this._sameProcessAsFrameLoader.get()
      );
    }

    get loadContext() {
      if (this._loadContext) {
        return this._loadContext;
      }

      let { frameLoader } = this;
      if (!frameLoader) {
        return null;
      }
      this._loadContext = frameLoader.loadContext;
      return this._loadContext;
    }

    get autoCompletePopup() {
      return document.getElementById(this.getAttribute("autocompletepopup"));
    }

    get dateTimePicker() {
      return document.getElementById(this.getAttribute("datetimepicker"));
    }

    /**
     * Provides a node to hang popups (such as the datetimepicker) from.
     * If this <browser> isn't the descendant of a <stack>, null is returned
     * instead and popup code must handle this case.
     */
    get popupAnchor() {
      let stack = this.closest("stack");
      if (!stack) {
        return null;
      }

      let popupAnchor = stack.querySelector(".popup-anchor");
      if (popupAnchor) {
        return popupAnchor;
      }

      // Create an anchor for the popup
      popupAnchor = document.createXULElement("hbox");
      popupAnchor.className = "popup-anchor";
      popupAnchor.hidden = true;
      stack.appendChild(popupAnchor);
      return popupAnchor;
    }

    set docShellIsActive(val) {
      if (this.isRemoteBrowser) {
        let { frameLoader } = this;
        if (frameLoader && frameLoader.remoteTab) {
          frameLoader.remoteTab.docShellIsActive = val;
        }
      } else if (this.docShell) {
        this.docShell.isActive = val;
      }
    }

    get docShellIsActive() {
      if (this.isRemoteBrowser) {
        let { frameLoader } = this;
        if (frameLoader && frameLoader.remoteTab) {
          return frameLoader.remoteTab.docShellIsActive;
        }
        return false;
      }
      return this.docShell && this.docShell.isActive;
    }

    set renderLayers(val) {
      if (this.isRemoteBrowser) {
        let { frameLoader } = this;
        if (frameLoader && frameLoader.remoteTab) {
          frameLoader.remoteTab.renderLayers = val;
        }
      } else {
        this.docShellIsActive = val;
      }
    }

    get renderLayers() {
      if (this.isRemoteBrowser) {
        let { frameLoader } = this;
        if (frameLoader && frameLoader.remoteTab) {
          return frameLoader.remoteTab.renderLayers;
        }
        return false;
      }
      return this.docShellIsActive;
    }

    get hasLayers() {
      if (this.isRemoteBrowser) {
        let { frameLoader } = this;
        if (frameLoader && frameLoader.remoteTab) {
          return frameLoader.remoteTab.hasLayers;
        }
        return false;
      }

      return this.docShellIsActive;
    }

    get imageDocument() {
      if (this.isRemoteBrowser) {
        return this._imageDocument;
      }
      var document = this.contentDocument;
      if (!document || !(document instanceof Ci.nsIImageDocument)) {
        return null;
      }

      try {
        return {
          width: document.imageRequest.image.width,
          height: document.imageRequest.image.height,
        };
      } catch (e) {}
      return null;
    }

    get isRemoteBrowser() {
      return this.getAttribute("remote") == "true";
    }

    get remoteType() {
      if (!this.isRemoteBrowser || !this.messageManager) {
        return null;
      }

      return this.messageManager.remoteType;
    }

    get isCrashed() {
      if (!this.isRemoteBrowser || !this.frameLoader) {
        return false;
      }

      return !this.frameLoader.remoteTab;
    }

    get messageManager() {
      // Bug 1524084 - Trying to get at the message manager while in the crashed state will
      // create a new message manager that won't shut down properly when the crashed browser
      // is removed from the DOM. We work around that right now by returning null if we're
      // in the crashed state.
      if (this.frameLoader && !this.isCrashed) {
        return this.frameLoader.messageManager;
      }
      return null;
    }

    get webBrowserFind() {
      if (!this._webBrowserFind) {
        this._webBrowserFind = this.docShell
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebBrowserFind);
      }
      return this._webBrowserFind;
    }

    get finder() {
      if (this.isRemoteBrowser) {
        if (!this._remoteFinder) {
          // Don't attempt to create the remote finder if the
          // messageManager has already gone away
          if (!this.messageManager) {
            return null;
          }

          let jsm = "resource://gre/modules/FinderParent.jsm";
          let { FinderParent } = ChromeUtils.import(jsm, {});
          this._remoteFinder = new FinderParent(this);
        }
        return this._remoteFinder;
      }
      if (!this._finder) {
        if (!this.docShell) {
          return null;
        }

        let Finder = ChromeUtils.import("resource://gre/modules/Finder.jsm", {})
          .Finder;
        this._finder = new Finder(this.docShell);
      }
      return this._finder;
    }

    get fastFind() {
      if (!this._fastFind) {
        if (!("@mozilla.org/typeaheadfind;1" in Cc)) {
          return null;
        }

        var tabBrowser = this.getTabBrowser();
        if (tabBrowser && "fastFind" in tabBrowser) {
          return (this._fastFind = tabBrowser.fastFind);
        }

        if (!this.docShell) {
          return null;
        }

        this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(
          Ci.nsITypeAheadFind
        );
        this._fastFind.init(this.docShell);
      }
      return this._fastFind;
    }

    get outerWindowID() {
      if (this.isRemoteBrowser) {
        return this._outerWindowID;
      }
      return this.docShell.outerWindowID;
    }

    get innerWindowID() {
      if (this.isRemoteBrowser) {
        return this._innerWindowID;
      }
      try {
        return this.contentWindow.windowUtils.currentInnerWindowID;
      } catch (e) {
        if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
          throw e;
        }
        return null;
      }
    }

    get browsingContext() {
      return this.frameLoader.browsingContext;
    }
    /**
     * Note that this overrides webNavigation on XULFrameElement, and duplicates the return value for the non-remote case
     */
    get webNavigation() {
      return this.isRemoteBrowser
        ? this._remoteWebNavigation
        : this.docShell && this.docShell.QueryInterface(Ci.nsIWebNavigation);
    }

    get webProgress() {
      return this.isRemoteBrowser
        ? this._remoteWebProgress
        : this.docShell &&
            this.docShell
              .QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIWebProgress);
    }

    get sessionHistory() {
      return this.webNavigation.sessionHistory;
    }

    get markupDocumentViewer() {
      return this.docShell.contentViewer;
    }

    get contentTitle() {
      return this.isRemoteBrowser
        ? this._contentTitle
        : this.contentDocument.title;
    }

    set characterSet(val) {
      if (this.isRemoteBrowser) {
        this.sendMessageToActor(
          "UpdateCharacterSet",
          { value: val },
          "BrowserTab"
        );
        this._characterSet = val;
      } else {
        this.docShell.charset = val;
        this.docShell.gatherCharsetMenuTelemetry();
      }
    }

    get characterSet() {
      return this.isRemoteBrowser ? this._characterSet : this.docShell.charset;
    }

    get mayEnableCharacterEncodingMenu() {
      return this.isRemoteBrowser
        ? this._mayEnableCharacterEncodingMenu
        : this.docShell.mayEnableCharacterEncodingMenu;
    }

    set mayEnableCharacterEncodingMenu(aMayEnable) {
      if (this.isRemoteBrowser) {
        this._mayEnableCharacterEncodingMenu = aMayEnable;
      }
    }

    get charsetAutodetected() {
      return this.isRemoteBrowser
        ? this._charsetAutodetected
        : this.docShell.charsetAutodetected;
    }

    set charsetAutodetected(aAutodetected) {
      if (this.isRemoteBrowser) {
        this._charsetAutodetected = aAutodetected;
      }
    }

    get contentPrincipal() {
      return this.isRemoteBrowser
        ? this._contentPrincipal
        : this.contentDocument.nodePrincipal;
    }

    get contentStoragePrincipal() {
      return this.isRemoteBrowser
        ? this._contentStoragePrincipal
        : this.contentDocument.effectiveStoragePrincipal;
    }

    get contentBlockingAllowListPrincipal() {
      return this.isRemoteBrowser
        ? this._contentBlockingAllowListPrincipal
        : this.contentDocument.contentBlockingAllowListPrincipal;
    }

    get csp() {
      return this.isRemoteBrowser ? this._csp : this.contentDocument.csp;
    }

    get contentRequestContextID() {
      if (this.isRemoteBrowser) {
        return this._contentRequestContextID;
      }
      try {
        return this.contentDocument.documentLoadGroup.requestContextID;
      } catch (e) {
        return null;
      }
    }

    set showWindowResizer(val) {
      if (val) {
        this.setAttribute("showresizer", "true");
      } else {
        this.removeAttribute("showresizer");
      }
    }

    get showWindowResizer() {
      return this.getAttribute("showresizer") == "true";
    }

    set fullZoom(val) {
      if (this.isRemoteBrowser) {
        let changed = val.toFixed(2) != this._fullZoom.toFixed(2);

        if (changed) {
          this._fullZoom = val;
          this.sendMessageToActor("FullZoom", { value: val }, "Zoom", true);

          let event = new Event("FullZoomChange", { bubbles: true });
          this.dispatchEvent(event);
        }
      } else {
        this.markupDocumentViewer.fullZoom = val;
      }
    }

    get referrerInfo() {
      return this.isRemoteBrowser
        ? this._referrerInfo
        : this.contentDocument.referrerInfo;
    }

    get fullZoom() {
      if (this.isRemoteBrowser) {
        return this._fullZoom;
      }
      return this.markupDocumentViewer.fullZoom;
    }

    set textZoom(val) {
      if (this.isRemoteBrowser) {
        let changed = val.toFixed(2) != this._textZoom.toFixed(2);

        if (changed) {
          this._textZoom = val;
          this.sendMessageToActor("TextZoom", { value: val }, "Zoom", true);

          let event = new Event("TextZoomChange", { bubbles: true });
          this.dispatchEvent(event);
        }
      } else {
        this.markupDocumentViewer.textZoom = val;
      }
    }

    get textZoom() {
      if (this.isRemoteBrowser) {
        return this._textZoom;
      }
      return this.markupDocumentViewer.textZoom;
    }

    get isSyntheticDocument() {
      if (this.isRemoteBrowser) {
        return this._isSyntheticDocument;
      }
      return this.contentDocument.mozSyntheticDocument;
    }

    get hasContentOpener() {
      if (this.isRemoteBrowser) {
        return this.frameLoader.remoteTab.hasContentOpener;
      }
      return !!this.contentWindow.opener;
    }

    get mStrBundle() {
      if (!this._mStrBundle) {
        // need to create string bundle manually instead of using <xul:stringbundle/>
        // see bug 63370 for details
        this._mStrBundle = Services.strings.createBundle(
          "chrome://global/locale/browser.properties"
        );
      }
      return this._mStrBundle;
    }

    get audioMuted() {
      return this._audioMuted;
    }

    get shouldHandleUnselectedTabHover() {
      return this._shouldSendUnselectedTabHover;
    }

    get securityUI() {
      if (this.isRemoteBrowser) {
        if (!this._securityUI) {
          // Don't attempt to create the remote web progress if the
          // messageManager has already gone away
          if (!this.messageManager) {
            return null;
          }

          let jsm = "resource://gre/modules/RemoteSecurityUI.jsm";
          let RemoteSecurityUI = ChromeUtils.import(jsm, {}).RemoteSecurityUI;
          this._securityUI = new RemoteSecurityUI();
        }

        // We want to double-wrap the JS implemented interface, so that QI and instanceof works.
        var ptr = Cc[
          "@mozilla.org/supports-interface-pointer;1"
        ].createInstance(Ci.nsISupportsInterfacePointer);
        ptr.data = this._securityUI;
        return ptr.data.QueryInterface(Ci.nsISecureBrowserUI);
      }

      if (!this.docShell.securityUI) {
        const SECUREBROWSERUI_CONTRACTID = "@mozilla.org/secure_browser_ui;1";
        var securityUI = Cc[SECUREBROWSERUI_CONTRACTID].createInstance(
          Ci.nsISecureBrowserUI
        );
        securityUI.init(this.docShell);
      }

      return this.docShell.securityUI;
    }

    set userTypedValue(val) {
      this.urlbarChangeTracker.userTyped();
      this._userTypedValue = val;
    }

    get userTypedValue() {
      return this._userTypedValue;
    }

    get dontPromptAndDontUnload() {
      return 1;
    }

    get dontPromptAndUnload() {
      return 2;
    }

    _wrapURIChangeCall(fn) {
      if (!this.isRemoteBrowser) {
        this.isNavigating = true;
        try {
          fn();
        } finally {
          this.isNavigating = false;
        }
      } else {
        fn();
      }
    }

    goBack() {
      var webNavigation = this.webNavigation;
      if (webNavigation.canGoBack) {
        this._wrapURIChangeCall(() => webNavigation.goBack());
      }
    }

    goForward() {
      var webNavigation = this.webNavigation;
      if (webNavigation.canGoForward) {
        this._wrapURIChangeCall(() => webNavigation.goForward());
      }
    }

    reload() {
      const nsIWebNavigation = Ci.nsIWebNavigation;
      const flags = nsIWebNavigation.LOAD_FLAGS_NONE;
      this.reloadWithFlags(flags);
    }

    reloadWithFlags(aFlags) {
      this.webNavigation.reload(aFlags);
    }

    stop() {
      const nsIWebNavigation = Ci.nsIWebNavigation;
      const flags = nsIWebNavigation.STOP_ALL;
      this.webNavigation.stop(flags);
    }

    /**
     * throws exception for unknown schemes
     */
    loadURI(aURI, aParams = {}) {
      if (!aURI) {
        aURI = "about:blank";
      }
      let {
        referrerInfo,
        triggeringPrincipal,
        postData,
        headers,
        csp,
      } = aParams;
      let loadFlags =
        aParams.loadFlags ||
        aParams.flags ||
        Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      let loadURIOptions = {
        triggeringPrincipal,
        csp,
        referrerInfo,
        loadFlags,
        postData,
        headers,
      };
      this._wrapURIChangeCall(() =>
        this.webNavigation.loadURI(aURI, loadURIOptions)
      );
    }

    gotoIndex(aIndex) {
      this._wrapURIChangeCall(() => this.webNavigation.gotoIndex(aIndex));
    }

    /**
     * Used by session restore to ensure that currentURI is set so
     * that switch-to-tab works before the tab is fully
     * restored. This function also invokes onLocationChanged
     * listeners in tabbrowser.xml.
     */
    _setCurrentURI(aURI) {
      if (this.isRemoteBrowser) {
        this._remoteWebProgressManager.setCurrentURI(aURI);
      } else {
        this.docShell.setCurrentURI(aURI);
      }
    }

    preserveLayers(preserve) {
      if (!this.isRemoteBrowser) {
        return;
      }
      let { frameLoader } = this;
      if (frameLoader.remoteTab) {
        frameLoader.remoteTab.preserveLayers(preserve);
      }
    }

    deprioritize() {
      if (!this.isRemoteBrowser) {
        return;
      }
      let { frameLoader } = this;
      if (frameLoader.remoteTab) {
        frameLoader.remoteTab.deprioritize();
      }
    }

    forceRepaint() {
      if (!this.isRemoteBrowser) {
        return;
      }
      let { frameLoader } = this;
      if (frameLoader && frameLoader.remoteTab) {
        frameLoader.remoteTab.forceRepaint();
      }
    }

    getTabBrowser() {
      if (
        this.ownerGlobal.gBrowser &&
        this.ownerGlobal.gBrowser.getTabForBrowser &&
        this.ownerGlobal.gBrowser.getTabForBrowser(this)
      ) {
        return this.ownerGlobal.gBrowser;
      }
      return null;
    }

    addProgressListener(aListener, aNotifyMask) {
      if (!aNotifyMask) {
        aNotifyMask = Ci.nsIWebProgress.NOTIFY_ALL;
      }
      this.webProgress.addProgressListener(aListener, aNotifyMask);
    }

    removeProgressListener(aListener) {
      this.webProgress.removeProgressListener(aListener);
    }

    onPageHide(aEvent) {
      if (!this.docShell || !this.fastFind) {
        return;
      }
      var tabBrowser = this.getTabBrowser();
      if (
        !tabBrowser ||
        !("fastFind" in tabBrowser) ||
        tabBrowser.selectedBrowser == this
      ) {
        this.fastFind.setDocShell(this.docShell);
      }
    }

    updateBlockedPopups() {
      let event = document.createEvent("Events");
      event.initEvent("DOMUpdateBlockedPopups", true, true);
      this.dispatchEvent(event);
    }

    retrieveListOfBlockedPopups() {
      this.messageManager.sendAsyncMessage(
        "PopupBlocking:GetBlockedPopupList",
        null
      );
      return new Promise(resolve => {
        let self = this;
        this.messageManager.addMessageListener(
          "PopupBlocking:ReplyGetBlockedPopupList",
          function replyReceived(msg) {
            self.messageManager.removeMessageListener(
              "PopupBlocking:ReplyGetBlockedPopupList",
              replyReceived
            );
            resolve(msg.data.popupData);
          }
        );
      });
    }

    unblockPopup(aPopupIndex) {
      this.messageManager.sendAsyncMessage("PopupBlocking:UnblockPopup", {
        index: aPopupIndex,
      });
    }

    audioPlaybackStarted() {
      if (this._audioMuted) {
        return;
      }
      let event = document.createEvent("Events");
      event.initEvent("DOMAudioPlaybackStarted", true, false);
      this.dispatchEvent(event);
    }

    audioPlaybackStopped() {
      let event = document.createEvent("Events");
      event.initEvent("DOMAudioPlaybackStopped", true, false);
      this.dispatchEvent(event);
    }

    /**
     * When the pref "media.block-autoplay-until-in-foreground" is on,
     * Gecko delays starting playback of media resources in tabs until the
     * tab has been in the foreground or resumed by tab's play tab icon.
     * - When Gecko delays starting playback of a media resource in a window,
     * it sends a message to call activeMediaBlockStarted(). This causes the
     * tab audio indicator to show.
     * - When a tab is foregrounded, Gecko starts playing all delayed media
     * resources in that tab, and sends a message to call
     * activeMediaBlockStopped(). This causes the tab audio indicator to hide.
     */
    activeMediaBlockStarted() {
      this._hasAnyPlayingMediaBeenBlocked = true;
      let event = document.createEvent("Events");
      event.initEvent("DOMAudioPlaybackBlockStarted", true, false);
      this.dispatchEvent(event);
    }

    activeMediaBlockStopped() {
      if (!this._hasAnyPlayingMediaBeenBlocked) {
        return;
      }
      this._hasAnyPlayingMediaBeenBlocked = false;
      let event = document.createEvent("Events");
      event.initEvent("DOMAudioPlaybackBlockStopped", true, false);
      this.dispatchEvent(event);
    }

    mute(transientState) {
      if (!transientState) {
        this._audioMuted = true;
      }
      let context = this.frameLoader.browsingContext;
      context.notifyMediaMutedChanged(true);
    }

    unmute() {
      this._audioMuted = false;
      let context = this.frameLoader.browsingContext;
      context.notifyMediaMutedChanged(false);
    }

    pauseMedia(disposable) {
      let suspendedReason;
      if (disposable) {
        suspendedReason = "mediaControlPaused";
      } else {
        suspendedReason = "lostAudioFocusTransiently";
      }

      this.sendMessageToActor(
        "AudioPlayback",
        { type: suspendedReason },
        "AudioPlayback",
        true
      );
    }

    stopMedia() {
      this.sendMessageToActor(
        "AudioPlayback",
        { type: "mediaControlStopped" },
        "AudioPlayback",
        true
      );
    }

    resumeMedia() {
      this.frameLoader.browsingContext.notifyStartDelayedAutoplayMedia();
      if (this._hasAnyPlayingMediaBeenBlocked) {
        this._hasAnyPlayingMediaBeenBlocked = false;
        let event = document.createEvent("Events");
        event.initEvent("DOMAudioPlaybackBlockStopped", true, false);
        this.dispatchEvent(event);
      }
    }

    unselectedTabHover(hovered) {
      if (!this._shouldSendUnselectedTabHover) {
        return;
      }
      this.messageManager.sendAsyncMessage("Browser:UnselectedTabHover", {
        hovered,
      });
    }

    didStartLoadSinceLastUserTyping() {
      return (
        !this.isNavigating &&
        this.urlbarChangeTracker._startedLoadSinceLastUserTyping
      );
    }

    construct() {
      elementsToDestroyOnUnload.add(this);
      this.resetFields();
      this.mInitialized = true;
      if (this.isRemoteBrowser) {
        /*
         * Don't try to send messages from this function. The message manager for
         * the <browser> element may not be initialized yet.
         */

        this._remoteWebNavigation = Cc[
          "@mozilla.org/remote-web-navigation;1"
        ].createInstance(Ci.nsIWebNavigation);
        this._remoteWebNavigationImpl = this._remoteWebNavigation.wrappedJSObject;
        this._remoteWebNavigationImpl.swapBrowser(this);

        // Initialize contentPrincipal to the about:blank principal for this loadcontext
        let { Services } = ChromeUtils.import(
          "resource://gre/modules/Services.jsm"
        );
        let aboutBlank = Services.io.newURI("about:blank");
        let ssm = Services.scriptSecurityManager;
        this._contentPrincipal = ssm.getLoadContextContentPrincipal(
          aboutBlank,
          this.loadContext
        );
        // CSP for about:blank is null; if we ever change _contentPrincipal above,
        // we should re-evaluate the CSP here.
        this._csp = null;

        this.messageManager.addMessageListener("Browser:Init", this);
        this.messageManager.addMessageListener("DOMTitleChanged", this);
        this.messageManager.addMessageListener("ImageDocumentLoaded", this);

        // browser-child messages, such as Content:LocationChange, are handled in
        // RemoteWebProgress, ensure it is loaded and ready.
        let jsm = "resource://gre/modules/RemoteWebProgress.jsm";
        let { RemoteWebProgressManager } = ChromeUtils.import(jsm, {});

        let oldManager = this._remoteWebProgressManager;
        this._remoteWebProgressManager = new RemoteWebProgressManager(this);
        if (oldManager) {
          // We're transitioning from one remote type to another. This means that
          // the RemoteWebProgress listener is listening to the old message manager,
          // and needs to be pointed at the new one.
          this._remoteWebProgressManager.swapListeners(oldManager);
        }

        this._remoteWebProgress = this._remoteWebProgressManager.topLevelWebProgress;

        this.messageManager.loadFrameScript(
          "chrome://global/content/browser-child.js",
          true
        );

        if (!this.hasAttribute("disablehistory")) {
          Services.obs.addObserver(
            this.observer,
            "browser:purge-session-history",
            true
          );
        }

        let rc_js = "resource://gre/modules/RemoteController.js";
        let scope = {};
        Services.scriptloader.loadSubScript(rc_js, scope);
        let RemoteController = scope.RemoteController;
        this._controller = new RemoteController(this);
        this.controllers.appendController(this._controller);
      }

      try {
        // |webNavigation.sessionHistory| will have been set by the frame
        // loader when creating the docShell as long as this xul:browser
        // doesn't have the 'disablehistory' attribute set.
        if (this.docShell && this.webNavigation.sessionHistory) {
          Services.obs.addObserver(
            this.observer,
            "browser:purge-session-history",
            true
          );

          // enable global history if we weren't told otherwise
          if (
            !this.hasAttribute("disableglobalhistory") &&
            !this.isRemoteBrowser
          ) {
            try {
              this.docShell.useGlobalHistory = true;
            } catch (ex) {
              // This can occur if the Places database is locked
              Cu.reportError("Error enabling browser global history: " + ex);
            }
          }
        }
      } catch (e) {
        Cu.reportError(e);
      }
      try {
        // Ensures the securityUI is initialized.
        var securityUI = this.securityUI; // eslint-disable-line no-unused-vars
      } catch (e) {}

      // tabbrowser.xml sets "sameProcessAsFrameLoader" as a direct property
      // on some browsers before they are put into a DOM (and get a
      // binding).  This hack makes sure that we hold a weak reference to
      // the other browser (and go through the proper getter and setter).
      if (this.hasOwnProperty("sameProcessAsFrameLoader")) {
        var sameProcessAsFrameLoader = this.sameProcessAsFrameLoader;
        delete this.sameProcessAsFrameLoader;
        this.sameProcessAsFrameLoader = sameProcessAsFrameLoader;
      }

      if (!this.isRemoteBrowser) {
        // If we've transitioned from remote to non-remote, we'll give up trying to
        // keep the web progress listeners persisted during the transition.
        delete this._remoteWebProgressManager;
        delete this._remoteWebProgress;

        this.addEventListener("pagehide", this.onPageHide, true);
      }

      if (this.messageManager) {
        this.messageManager.addMessageListener(
          "PopupBlocking:UpdateBlockedPopups",
          this
        );
        this.messageManager.addMessageListener("Autoscroll:Start", this);
        this.messageManager.addMessageListener("Autoscroll:Cancel", this);
        this.messageManager.addMessageListener(
          "UnselectedTabHover:Toggle",
          this
        );
      }
    }

    /**
     * This is necessary because the destructor doesn't always get called when
     * we are removed from a tabbrowser. This will be explicitly called by tabbrowser.
     */
    destroy() {
      elementsToDestroyOnUnload.delete(this);

      // Make sure that any open select is closed.
      if (this.hasAttribute("selectmenulist")) {
        let menulist = document.getElementById(
          this.getAttribute("selectmenulist")
        );
        if (menulist && menulist.open) {
          let resourcePath = "resource://gre/actors/SelectParent.jsm";
          let { SelectParentHelper } = ChromeUtils.import(resourcePath);
          SelectParentHelper.hide(menulist, this);
        }
      }

      this.resetFields();

      if (!this.mInitialized) {
        return;
      }

      this.mInitialized = false;

      if (this.isRemoteBrowser) {
        try {
          this.controllers.removeController(this._controller);
        } catch (ex) {
          // This can fail when this browser element is not attached to a
          // BrowserDOMWindow.
        }
        return;
      }

      this.lastURI = null;

      if (!this.isRemoteBrowser) {
        this.removeEventListener("pagehide", this.onPageHide, true);
      }

      if (this._autoScrollNeedsCleanup) {
        // we polluted the global scope, so clean it up
        this._autoScrollPopup.remove();
      }
    }

    /**
     * We call this _receiveMessage (and alias receiveMessage to it) so that
     * bindings that inherit from this one can delegate to it.
     */
    _receiveMessage(aMessage) {
      let data = aMessage.data;
      switch (aMessage.name) {
        case "PopupBlocking:UpdateBlockedPopups": {
          this.blockedPopups = {
            length: data.count,
            reported: !data.freshPopup,
          };

          this.updateBlockedPopups();
          break;
        }
        case "Autoscroll:Start": {
          if (!this.autoscrollEnabled) {
            return { autoscrollEnabled: false, usingApz: false };
          }
          this.startScroll(data.scrolldir, data.screenX, data.screenY);
          let usingApz = false;
          if (
            this.isRemoteBrowser &&
            data.scrollId != null &&
            this.mPrefs.getBoolPref("apz.autoscroll.enabled", false)
          ) {
            let { remoteTab } = this.frameLoader;
            if (remoteTab) {
              // If APZ is handling the autoscroll, it may decide to cancel
              // it of its own accord, so register an observer to allow it
              // to notify us of that.
              var os = Services.obs;
              os.addObserver(this.observer, "apz:cancel-autoscroll", true);

              usingApz = remoteTab.startApzAutoscroll(
                data.screenX,
                data.screenY,
                data.scrollId,
                data.presShellId
              );
            }
            // Save the IDs for later
            this._autoScrollScrollId = data.scrollId;
            this._autoScrollPresShellId = data.presShellId;
          }
          return { autoscrollEnabled: true, usingApz };
        }
        case "Autoscroll:Cancel":
          this._autoScrollPopup.hidePopup();
          break;
        case "UnselectedTabHover:Toggle":
          this._shouldSendUnselectedTabHover = data.enable
            ? ++this._unselectedTabHoverMessageListenerCount > 0
            : --this._unselectedTabHoverMessageListenerCount == 0;
          break;
      }
      return undefined;
    }

    receiveMessage(aMessage) {
      if (!this.isRemoteBrowser) {
        return this._receiveMessage(aMessage);
      }

      let data = aMessage.data;
      switch (aMessage.name) {
        case "Browser:Init":
          this._outerWindowID = data.outerWindowID;
          break;
        case "DOMTitleChanged":
          this._contentTitle = data.title;
          break;
        case "ImageDocumentLoaded":
          this._imageDocument = {
            width: data.width,
            height: data.height,
          };
          break;
        default:
          return this._receiveMessage(aMessage);
      }
      return undefined;
    }

    enableDisableCommandsRemoteOnly(
      aAction,
      aEnabledCommands,
      aDisabledCommands
    ) {
      if (this._controller) {
        this._controller.enableDisableCommands(
          aAction,
          aEnabledCommands,
          aDisabledCommands
        );
      }
    }

    updateSecurityUIForContentBlockingEvent(aEvent) {
      if (this.isRemoteBrowser && this.messageManager) {
        // Invoking this getter triggers the generation of the underlying object,
        // which we need to access with ._securityUI, because .securityUI returns
        // a wrapper that makes _update inaccessible.
        void this.securityUI;
        this._securityUI._updateContentBlockingEvent(aEvent);
      }
    }

    get remoteWebProgressManager() {
      return this._remoteWebProgressManager;
    }

    updateForStateChange(aCharset, aDocumentURI, aContentType) {
      if (this.isRemoteBrowser && this.messageManager) {
        if (aCharset != null) {
          this._characterSet = aCharset;
        }

        if (aDocumentURI != null) {
          this._documentURI = aDocumentURI;
        }

        if (aContentType != null) {
          this._documentContentType = aContentType;
        }
      }
    }

    updateWebNavigationForLocationChange(aCanGoBack, aCanGoForward) {
      if (this.isRemoteBrowser && this.messageManager) {
        let remoteWebNav = this._remoteWebNavigationImpl;
        remoteWebNav.canGoBack = aCanGoBack;
        remoteWebNav.canGoForward = aCanGoForward;
      }
    }

    updateForLocationChange(
      aLocation,
      aCharset,
      aMayEnableCharacterEncodingMenu,
      aCharsetAutodetected,
      aDocumentURI,
      aTitle,
      aContentPrincipal,
      aContentStoragePrincipal,
      aContentBlockingAllowListPrincipal,
      aCSP,
      aReferrerInfo,
      aIsSynthetic,
      aInnerWindowID,
      aHaveRequestContextID,
      aRequestContextID,
      aContentType
    ) {
      if (this.isRemoteBrowser && this.messageManager) {
        if (aCharset != null) {
          this._characterSet = aCharset;
          this._mayEnableCharacterEncodingMenu = aMayEnableCharacterEncodingMenu;
          this._charsetAutodetected = aCharsetAutodetected;
        }

        if (aContentType != null) {
          this._documentContentType = aContentType;
        }

        this._remoteWebNavigationImpl._currentURI = aLocation;
        this._documentURI = aDocumentURI;
        this._contentTile = aTitle;
        this._imageDocument = null;
        this._contentPrincipal = aContentPrincipal;
        this._contentStoragePrincipal = aContentStoragePrincipal;
        this._contentBlockingAllowListPrincipal = aContentBlockingAllowListPrincipal;
        this._csp = aCSP;
        this._referrerInfo = aReferrerInfo;
        this._isSyntheticDocument = aIsSynthetic;
        this._innerWindowID = aInnerWindowID;
        this._contentRequestContextID = aHaveRequestContextID
          ? aRequestContextID
          : null;
      }
    }

    purgeSessionHistory() {
      if (this.isRemoteBrowser) {
        try {
          this.messageManager.sendAsyncMessage("Browser:PurgeSessionHistory");
        } catch (ex) {
          // This can throw if the browser has started to go away.
          if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
            throw ex;
          }
        }
        this._remoteWebNavigationImpl.canGoBack = false;
        this._remoteWebNavigationImpl.canGoForward = false;
        return;
      }
      this.messageManager.sendAsyncMessage("Browser:PurgeSessionHistory");
    }

    createAboutBlankContentViewer(aPrincipal, aStoragePrincipal) {
      if (this.isRemoteBrowser) {
        // Ensure that the content process has the permissions which are
        // needed to create a document with the given principal.
        let permissionPrincipal = BrowserUtils.principalWithMatchingOA(
          aPrincipal,
          this.contentPrincipal
        );
        this.frameLoader.remoteTab.transmitPermissionsForPrincipal(
          permissionPrincipal
        );

        // This still uses the message manager, for the following reasons:
        //
        // 1. Due to bug 1523638, it's virtually certain that, if we've just created
        //    this <xul:browser>, that the WindowGlobalParent for the top-level frame
        //    of this browser doesn't exist yet, so it's not possible to get at a
        //    JS Window Actor for it.
        //
        // 2. JS Window Actors are tied to the principals for the frames they're running
        //    in - switching principals is therefore self-destructive and unexpected.
        //
        // So we'll continue to use the message manager until we come up with a better
        // solution.
        this.messageManager.sendAsyncMessage(
          "BrowserElement:CreateAboutBlank",
          { principal: aPrincipal, storagePrincipal: aStoragePrincipal }
        );
        return;
      }
      let principal = BrowserUtils.principalWithMatchingOA(
        aPrincipal,
        this.contentPrincipal
      );
      let storagePrincipal = BrowserUtils.principalWithMatchingOA(
        aStoragePrincipal,
        this.contentStoragePrincipal
      );
      this.docShell.createAboutBlankContentViewer(principal, storagePrincipal);
    }

    stopScroll() {
      if (this._scrolling) {
        this._scrolling = false;
        window.removeEventListener("mousemove", this, true);
        window.removeEventListener("mousedown", this, true);
        window.removeEventListener("mouseup", this, true);
        window.removeEventListener("DOMMouseScroll", this, true);
        window.removeEventListener("contextmenu", this, true);
        window.removeEventListener("keydown", this, true);
        window.removeEventListener("keypress", this, true);
        window.removeEventListener("keyup", this, true);
        this.messageManager.sendAsyncMessage("Autoscroll:Stop");

        try {
          Services.obs.removeObserver(this.observer, "apz:cancel-autoscroll");
        } catch (ex) {
          // It's not clear why this sometimes throws an exception
        }

        if (this.isRemoteBrowser && this._autoScrollScrollId != null) {
          let { remoteTab } = this.frameLoader;
          if (remoteTab) {
            remoteTab.stopApzAutoscroll(
              this._autoScrollScrollId,
              this._autoScrollPresShellId
            );
          }
          this._autoScrollScrollId = null;
          this._autoScrollPresShellId = null;
        }
      }
    }

    _createAutoScrollPopup() {
      var popup = document.createXULElement("panel");
      popup.className = "autoscroller";
      // We set this attribute on the element so that mousemove
      // events can be handled by browser-content.js.
      popup.setAttribute("mousethrough", "always");
      popup.setAttribute("consumeoutsideclicks", "true");
      popup.setAttribute("rolluponmousewheel", "true");
      popup.setAttribute("hidden", "true");
      return popup;
    }

    startScroll(scrolldir, screenX, screenY) {
      const POPUP_SIZE = 32;
      if (!this._autoScrollPopup) {
        if (this.hasAttribute("autoscrollpopup")) {
          // our creator provided a popup to share
          this._autoScrollPopup = document.getElementById(
            this.getAttribute("autoscrollpopup")
          );
        } else {
          // we weren't provided a popup; we have to use the global scope
          this._autoScrollPopup = this._createAutoScrollPopup();
          document.documentElement.appendChild(this._autoScrollPopup);
          this._autoScrollNeedsCleanup = true;
        }
        this._autoScrollPopup.removeAttribute("hidden");
        this._autoScrollPopup.setAttribute("noautofocus", "true");
        this._autoScrollPopup.style.height = POPUP_SIZE + "px";
        this._autoScrollPopup.style.width = POPUP_SIZE + "px";
        this._autoScrollPopup.style.margin = -POPUP_SIZE / 2 + "px";
      }

      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(
        Ci.nsIScreenManager
      );
      let screen = screenManager.screenForRect(screenX, screenY, 1, 1);

      // we need these attributes so themers don't need to create per-platform packages
      if (screen.colorDepth > 8) {
        // need high color for transparency
        // Exclude second-rate platforms
        this._autoScrollPopup.setAttribute(
          "transparent",
          !/BeOS|OS\/2/.test(navigator.appVersion)
        );
        // Enable translucency on Windows and Mac
        this._autoScrollPopup.setAttribute(
          "translucent",
          AppConstants.platform == "win" || AppConstants.platform == "macosx"
        );
      }

      this._autoScrollPopup.setAttribute("scrolldir", scrolldir);
      this._autoScrollPopup.addEventListener("popuphidden", this, true);

      // Sanitize screenX/screenY for available screen size with half the size
      // of the popup removed. The popup uses negative margins to center on the
      // coordinates we pass. Unfortunately `window.screen.availLeft` can be negative
      // on Windows in multi-monitor setups, so we use nsIScreenManager instead:
      let left = {},
        top = {},
        width = {},
        height = {};
      screen.GetAvailRect(left, top, width, height);

      // We need to get screen CSS-pixel (rather than display-pixel) coordinates.
      // With 175% DPI, the actual ratio of display pixels to CSS pixels is
      // 1.7647 because of rounding inside gecko. Unfortunately defaultCSSScaleFactor
      // returns the original 1.75 dpi factor. While window.devicePixelRatio would
      // get us the correct ratio, if the window is split between 2 screens,
      // window.devicePixelRatio isn't guaranteed to match the screen we're
      // autoscrolling on. So instead we do the same math as Gecko.
      const scaleFactor = 60 / Math.round(60 / screen.defaultCSSScaleFactor);
      let minX = left.value / scaleFactor + 0.5 * POPUP_SIZE;
      let maxX = (left.value + width.value) / scaleFactor - 0.5 * POPUP_SIZE;
      let minY = top.value / scaleFactor + 0.5 * POPUP_SIZE;
      let maxY = (top.value + height.value) / scaleFactor - 0.5 * POPUP_SIZE;
      let popupX = Math.max(minX, Math.min(maxX, screenX));
      let popupY = Math.max(minY, Math.min(maxY, screenY));
      this._autoScrollPopup.openPopupAtScreen(popupX, popupY);
      this._ignoreMouseEvents = true;
      this._scrolling = true;
      this._startX = screenX;
      this._startY = screenY;

      window.addEventListener("mousemove", this, true);
      window.addEventListener("mousedown", this, true);
      window.addEventListener("mouseup", this, true);
      window.addEventListener("DOMMouseScroll", this, true);
      window.addEventListener("contextmenu", this, true);
      window.addEventListener("keydown", this, true);
      window.addEventListener("keypress", this, true);
      window.addEventListener("keyup", this, true);
    }

    handleEvent(aEvent) {
      if (this._scrolling) {
        switch (aEvent.type) {
          case "mousemove": {
            var x = aEvent.screenX - this._startX;
            var y = aEvent.screenY - this._startY;

            if (
              x > this._AUTOSCROLL_SNAP ||
              x < -this._AUTOSCROLL_SNAP ||
              (y > this._AUTOSCROLL_SNAP || y < -this._AUTOSCROLL_SNAP)
            ) {
              this._ignoreMouseEvents = false;
            }
            break;
          }
          case "mouseup":
          case "mousedown":
          case "contextmenu": {
            if (!this._ignoreMouseEvents) {
              // Use a timeout to prevent the mousedown from opening the popup again.
              // Ideally, we could use preventDefault here, but contenteditable
              // and middlemouse paste don't interact well. See bug 1188536.
              setTimeout(() => this._autoScrollPopup.hidePopup(), 0);
            }
            this._ignoreMouseEvents = false;
            break;
          }
          case "DOMMouseScroll": {
            this._autoScrollPopup.hidePopup();
            aEvent.preventDefault();
            break;
          }
          case "popuphidden": {
            this._autoScrollPopup.removeEventListener(
              "popuphidden",
              this,
              true
            );
            this.stopScroll();
            break;
          }
          case "keydown": {
            if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
              // the escape key will be processed by
              // nsXULPopupManager::KeyDown and the panel will be closed.
              // So, don't consume the key event here.
              break;
            }
            // don't break here. we need to eat keydown events.
          }
          // fall through
          case "keypress":
          case "keyup": {
            // All keyevents should be eaten here during autoscrolling.
            aEvent.stopPropagation();
            aEvent.preventDefault();
            break;
          }
        }
      }
    }

    closeBrowser() {
      // The request comes from a XPCOM component, we'd want to redirect
      // the request to tabbrowser.
      let tabbrowser = this.getTabBrowser();
      if (tabbrowser) {
        let tab = tabbrowser.getTabForBrowser(this);
        if (tab) {
          tabbrowser.removeTab(tab);
          return;
        }
      }

      throw new Error(
        "Closing a browser which was not attached to a tabbrowser is unsupported."
      );
    }

    swapBrowsers(aOtherBrowser) {
      // The request comes from a XPCOM component, we'd want to redirect
      // the request to tabbrowser so tabbrowser will be setup correctly,
      // and it will eventually call swapDocShells.
      let ourTabBrowser = this.getTabBrowser();
      let otherTabBrowser = aOtherBrowser.getTabBrowser();
      if (ourTabBrowser && otherTabBrowser) {
        let ourTab = ourTabBrowser.getTabForBrowser(this);
        let otherTab = otherTabBrowser.getTabForBrowser(aOtherBrowser);
        ourTabBrowser.swapBrowsers(ourTab, otherTab);
        return;
      }

      // One of us is not connected to a tabbrowser, so just swap.
      this.swapDocShells(aOtherBrowser);
    }

    swapDocShells(aOtherBrowser) {
      if (this.isRemoteBrowser != aOtherBrowser.isRemoteBrowser) {
        throw new Error(
          "Can only swap docshells between browsers in the same process."
        );
      }

      // Give others a chance to swap state.
      // IMPORTANT: Since a swapDocShells call does not swap the messageManager
      //            instances attached to a browser to aOtherBrowser, others
      //            will need to add the message listeners to the new
      //            messageManager.
      //            This is not a bug in swapDocShells or the FrameLoader,
      //            merely a design decision: If message managers were swapped,
      //            so that no new listeners were needed, the new
      //            aOtherBrowser.messageManager would have listeners pointing
      //            to the JS global of the current browser, which would rather
      //            easily create leaks while swapping.
      // IMPORTANT2: When the current browser element is removed from DOM,
      //             which is quite common after a swpDocShells call, its
      //             frame loader is destroyed, and that destroys the relevant
      //             message manager, which will remove the listeners.
      let event = new CustomEvent("SwapDocShells", { detail: aOtherBrowser });
      this.dispatchEvent(event);
      event = new CustomEvent("SwapDocShells", { detail: this });
      aOtherBrowser.dispatchEvent(event);

      // We need to swap fields that are tied to our docshell or related to
      // the loaded page
      // Fields which are built as a result of notifactions (pageshow/hide,
      // DOMLinkAdded/Removed, onStateChange) should not be swapped here,
      // because these notifications are dispatched again once the docshells
      // are swapped.
      var fieldsToSwap = ["_webBrowserFind"];

      if (this.isRemoteBrowser) {
        fieldsToSwap.push(
          ...[
            "_remoteWebNavigation",
            "_remoteWebNavigationImpl",
            "_remoteWebProgressManager",
            "_remoteWebProgress",
            "_remoteFinder",
            "_securityUI",
            "_documentURI",
            "_documentContentType",
            "_contentTitle",
            "_characterSet",
            "_mayEnableCharacterEncodingMenu",
            "_charsetAutodetected",
            "_contentPrincipal",
            "_contentStoragePrincipal",
            "_contentBlockingAllowListPrincipal",
            "_imageDocument",
            "_fullZoom",
            "_textZoom",
            "_isSyntheticDocument",
            "_innerWindowID",
          ]
        );
      }

      var ourFieldValues = {};
      var otherFieldValues = {};
      for (let field of fieldsToSwap) {
        ourFieldValues[field] = this[field];
        otherFieldValues[field] = aOtherBrowser[field];
      }

      if (window.PopupNotifications) {
        PopupNotifications._swapBrowserNotifications(aOtherBrowser, this);
      }

      try {
        this.swapFrameLoaders(aOtherBrowser);
      } catch (ex) {
        // This may not be implemented for browser elements that are not
        // attached to a BrowserDOMWindow.
      }

      for (let field of fieldsToSwap) {
        this[field] = otherFieldValues[field];
        aOtherBrowser[field] = ourFieldValues[field];
      }

      if (!this.isRemoteBrowser) {
        // Null the current nsITypeAheadFind instances so that they're
        // lazily re-created on access. We need to do this because they
        // might have attached the wrong docShell.
        this._fastFind = aOtherBrowser._fastFind = null;
      } else {
        // Rewire the remote listeners
        this._remoteWebNavigationImpl.swapBrowser(this);
        aOtherBrowser._remoteWebNavigationImpl.swapBrowser(aOtherBrowser);

        if (
          this._remoteWebProgressManager &&
          aOtherBrowser._remoteWebProgressManager
        ) {
          this._remoteWebProgressManager.swapBrowser(this);
          aOtherBrowser._remoteWebProgressManager.swapBrowser(aOtherBrowser);
        }

        if (this._remoteFinder) {
          this._remoteFinder.swapBrowser(this);
        }
        if (aOtherBrowser._remoteFinder) {
          aOtherBrowser._remoteFinder.swapBrowser(aOtherBrowser);
        }
      }

      event = new CustomEvent("EndSwapDocShells", { detail: aOtherBrowser });
      this.dispatchEvent(event);
      event = new CustomEvent("EndSwapDocShells", { detail: this });
      aOtherBrowser.dispatchEvent(event);
    }

    getInPermitUnload(aCallback) {
      if (this.isRemoteBrowser) {
        let { remoteTab } = this.frameLoader;
        if (!remoteTab) {
          // If we're crashed, we're definitely not in this state anymore.
          aCallback(false);
          return;
        }
        aCallback(LazyModules.PermitUnloader.inPermitUnload(this.frameLoader));
        return;
      }

      if (!this.docShell || !this.docShell.contentViewer) {
        aCallback(false);
        return;
      }
      aCallback(this.docShell.contentViewer.inPermitUnload);
    }

    permitUnload(aPermitUnloadFlags) {
      if (this.isRemoteBrowser) {
        if (!LazyModules.PermitUnloader.hasBeforeUnload(this.frameLoader)) {
          return { permitUnload: true, timedOut: false };
        }

        return LazyModules.PermitUnloader.permitUnload(
          this.frameLoader,
          aPermitUnloadFlags
        );
      }

      if (!this.docShell || !this.docShell.contentViewer) {
        return { permitUnload: true, timedOut: false };
      }
      return {
        permitUnload: this.docShell.contentViewer.permitUnload(
          aPermitUnloadFlags
        ),
        timedOut: false,
      };
    }

    print(aOuterWindowID, aPrintSettings, aPrintProgressListener) {
      if (!this.frameLoader) {
        throw Components.Exception("No frame loader.", Cr.NS_ERROR_FAILURE);
      }

      this.frameLoader.print(
        aOuterWindowID,
        aPrintSettings,
        aPrintProgressListener
      );
    }

    drawSnapshot(x, y, w, h, scale, backgroundColor) {
      if (!this.frameLoader) {
        throw Components.Exception("No frame loader.", Cr.NS_ERROR_FAILURE);
      }
      return this.frameLoader.drawSnapshot(x, y, w, h, scale, backgroundColor);
    }

    dropLinks(aLinks, aTriggeringPrincipal) {
      if (!this.droppedLinkHandler) {
        return false;
      }
      let links = [];
      for (let i = 0; i < aLinks.length; i += 3) {
        links.push({
          url: aLinks[i],
          name: aLinks[i + 1],
          type: aLinks[i + 2],
        });
      }
      this.droppedLinkHandler(null, links, aTriggeringPrincipal);
      return true;
    }

    getContentBlockingLog() {
      if (this.isRemoteBrowser) {
        return this.frameLoader.remoteTab.getContentBlockingLog();
      }
      return this.docShell
        ? this.docShell.getContentBlockingLog()
        : Promise.reject("docshell isn't available");
    }

    // Send an asynchronous message to the remote child via an actor.
    // Note: use this only for messages through an actor. For old-style
    // messages, use the message manager. If 'all' is true, then send
    // a message to all descendant processes.
    sendMessageToActor(messageName, args, actorName, all) {
      if (!this.frameLoader) {
        return;
      }

      let windowGlobal = this.browsingContext.currentWindowGlobal;
      if (!windowGlobal) {
        // Workaround for bug 1523638 where about:blank is loaded in a tab.
        if (messageName == "Browser:AppTab") {
          setTimeout(() => {
            this.sendMessageToActor(messageName, args, actorName);
          }, 0);
        }
        return;
      }

      function sendToChildren(browsingContext, checkRoot) {
        let windowGlobal = browsingContext.currentWindowGlobal;
        if (windowGlobal && (!checkRoot || windowGlobal.isProcessRoot)) {
          windowGlobal.getActor(actorName).sendAsyncMessage(messageName, args);
        }

        if (all) {
          let contexts = browsingContext.getChildren();
          for (let context of contexts) {
            sendToChildren(context, true);
          }
        }
      }

      sendToChildren(this.browsingContext, false);
    }

    enterModalState() {
      this.sendMessageToActor("EnterModalState", {}, "BrowserElement", true);
    }

    leaveModalState() {
      this.sendMessageToActor("LeaveModalState", {}, "BrowserElement", true);
    }
  }

  MozXULElement.implementCustomInterface(MozBrowser, [Ci.nsIBrowser]);
  customElements.define("browser", MozBrowser);
}
