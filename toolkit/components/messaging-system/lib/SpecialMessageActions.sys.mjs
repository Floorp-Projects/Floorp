/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const DOH_DOORHANGER_DECISION_PREF = "doh-rollout.doorhanger-decision";
const NETWORK_TRR_MODE_PREF = "network.trr.mode";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  FxAccounts: "resource://gre/modules/FxAccounts.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
  UITour: "resource:///modules/UITour.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Spotlight: "resource://activity-stream/lib/Spotlight.jsm",
});

export const SpecialMessageActions = {
  // This is overridden by ASRouter.init
  blockMessageById() {
    throw new Error("ASRouter not intialized yet");
  },

  /**
   * loadAddonIconInURLBar - load addons-notification icon by displaying
   * box containing addons icon in urlbar. See Bug 1513882
   *
   * @param  {Browser} browser browser element for showing addons icon
   */
  loadAddonIconInURLBar(browser) {
    if (!browser) {
      return;
    }
    const chromeDoc = browser.ownerDocument;
    let notificationPopupBox = chromeDoc.getElementById(
      "notification-popup-box"
    );
    if (!notificationPopupBox) {
      return;
    }
    if (
      notificationPopupBox.style.display === "none" ||
      notificationPopupBox.style.display === ""
    ) {
      notificationPopupBox.style.display = "block";
    }
  },

  /**
   *
   * @param {Browser} browser The revelant Browser
   * @param {string} url URL to look up install location
   * @param {string} telemetrySource Telemetry information to pass to getInstallForURL
   */
  async installAddonFromURL(browser, url, telemetrySource = "amo") {
    try {
      this.loadAddonIconInURLBar(browser);
      const aUri = Services.io.newURI(url);
      const systemPrincipal =
        Services.scriptSecurityManager.getSystemPrincipal();

      // AddonManager installation source associated to the addons installed from activitystream's CFR
      // and RTAMO (source is going to be "amo" if not configured explicitly in the message provider).
      const telemetryInfo = { source: telemetrySource };
      const install = await lazy.AddonManager.getInstallForURL(aUri.spec, {
        telemetryInfo,
      });
      await lazy.AddonManager.installAddonFromWebpage(
        "application/x-xpinstall",
        browser,
        systemPrincipal,
        install
      );
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Pin Firefox to taskbar.
   *
   * @param {Window} window Reference to a window object
   * @param {boolean} pin Private Browsing Mode if true
   */
  pinFirefoxToTaskbar(window, privateBrowsing = false) {
    return window.getShellService().pinToTaskbar(privateBrowsing);
  },

  /**
   *  Set browser as the operating system default browser.
   *
   *  @param {Window} window Reference to a window object
   */
  async setDefaultBrowser(window) {
    await window.getShellService().setAsDefault();
  },

  /**
   * Set browser as the default PDF handler.
   *
   * @param {Window} window Reference to a window object
   */
  setDefaultPDFHandler(window, onlyIfKnownBrowser = false) {
    window.getShellService().setAsDefaultPDFHandler(onlyIfKnownBrowser);
  },

  /**
   * Reset browser homepage and newtab to default with a certain section configuration
   *
   * @param {"default"|null} home Value to set for browser homepage
   * @param {"default"|null} newtab Value to set for browser newtab
   * @param {obj} layout Configuration options for newtab sections
   * @returns {undefined}
   */
  configureHomepage({ homePage = null, newtab = null, layout = null }) {
    // Homepage can be default, blank or a custom url
    if (homePage === "default") {
      Services.prefs.clearUserPref("browser.startup.homepage");
    }
    // Newtab page can only be default or blank
    if (newtab === "default") {
      Services.prefs.clearUserPref("browser.newtabpage.enabled");
    }
    if (layout) {
      // Existing prefs that interact with the newtab page layout, we default to true
      // or payload configuration
      let newtabConfigurations = [
        [
          // controls the search bar
          "browser.newtabpage.activity-stream.showSearch",
          layout.search,
        ],
        [
          // controls the topsites
          "browser.newtabpage.activity-stream.feeds.topsites",
          layout.topsites,
          // User can control number of topsite rows
          ["browser.newtabpage.activity-stream.topSitesRows"],
        ],
        [
          // controls the highlights section
          "browser.newtabpage.activity-stream.feeds.section.highlights",
          layout.highlights,
          // User can control number of rows and highlight sources
          [
            "browser.newtabpage.activity-stream.section.highlights.rows",
            "browser.newtabpage.activity-stream.section.highlights.includeVisited",
            "browser.newtabpage.activity-stream.section.highlights.includePocket",
            "browser.newtabpage.activity-stream.section.highlights.includeDownloads",
            "browser.newtabpage.activity-stream.section.highlights.includeBookmarks",
          ],
        ],
        [
          // controls the topstories section
          "browser.newtabpage.activity-stream.feeds.system.topstories",
          layout.topstories,
        ],
      ].filter(
        // If a section has configs that the user changed we will skip that section
        ([, , sectionConfigs]) =>
          !sectionConfigs ||
          sectionConfigs.every(
            prefName => !Services.prefs.prefHasUserValue(prefName)
          )
      );

      for (let [prefName, prefValue] of newtabConfigurations) {
        Services.prefs.setBoolPref(prefName, prefValue);
      }
    }
  },

  /**
   * Set prefs with special message actions
   *
   * @param {Object} pref - A pref to be updated.
   * @param {string} pref.name - The name of the pref to be updated
   * @param {string} [pref.value] - The value of the pref to be updated. If not included, the pref will be reset.
   */
  setPref(pref) {
    // Array of prefs that are allowed to be edited by SET_PREF
    const allowedPrefs = [
      "browser.dataFeatureRecommendations.enabled",
      "browser.migrate.content-modal.about-welcome-behavior",
      "browser.migrate.content-modal.import-all.enabled",
      "browser.migrate.preferences-entrypoint.enabled",
      "browser.shopping.experience2023.active",
      "browser.shopping.experience2023.optedIn",
      "browser.shopping.experience2023.survey.optedInTime",
      "browser.shopping.experience2023.survey.hasSeen",
      "browser.shopping.experience2023.survey.pdpVisits",
      "browser.startup.homepage",
      "browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt",
      "browser.privateWindowSeparation.enabled",
      "browser.firefox-view.feature-tour",
      "browser.pdfjs.feature-tour",
      "browser.newtab.feature-tour",
      "cookiebanners.service.mode",
      "cookiebanners.service.mode.privateBrowsing",
      "cookiebanners.service.detectOnly",
      "messaging-system.askForFeedback",
    ];

    if (
      !allowedPrefs.includes(pref.name) &&
      !pref.name.startsWith("messaging-system-action.")
    ) {
      pref.name = `messaging-system-action.${pref.name}`;
    }
    // If pref has no value, reset it, otherwise set it to desired value
    switch (typeof pref.value) {
      case "object":
      case "undefined":
        Services.prefs.clearUserPref(pref.name);
        break;
      case "string":
        Services.prefs.setStringPref(pref.name, pref.value);
        break;
      case "number":
        Services.prefs.setIntPref(pref.name, pref.value);
        break;
      case "boolean":
        Services.prefs.setBoolPref(pref.name, pref.value);
        break;
      default:
        throw new Error(
          `Special message action with type SET_PREF, pref of "${pref.name}" is an unsupported type.`
        );
    }
  },

  /**
   * Open an FxA sign-in page and automatically close it once sign-in
   * completes.
   *
   * @param {any=} data
   * @param {Browser} browser the xul:browser rendering the page
   * @returns {Promise<boolean>} true if the user signed in, false otherwise
   */
  async fxaSignInFlow(data, browser) {
    if (!(await lazy.FxAccounts.canConnectAccount())) {
      return false;
    }
    const url = await lazy.FxAccounts.config.promiseConnectAccountURI(
      data?.entrypoint || "activity-stream-firstrun",
      data?.extraParams || {}
    );

    let window = browser.ownerGlobal;

    let fxaBrowser = await new Promise(resolve => {
      window.openLinkIn(url, data?.where || "tab", {
        private: false,
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
        csp: null,
        resolveOnContentBrowserCreated: resolve,
        forceForeground: true,
      });
    });

    let gBrowser = fxaBrowser.getTabBrowser();
    let fxaTab = gBrowser.getTabForBrowser(fxaBrowser);

    let didSignIn = await new Promise(resolve => {
      // We're going to be setting up a listener and an observer for this
      // mechanism.
      //
      // 1. An event listener for the TabClose event, to detect if the user
      //    closes the tab before completing sign-in
      // 2. An nsIObserver that listens for the UIState for FxA to reach
      //    STATUS_SIGNED_IN.
      //
      // We want to clean up both the listener and observer when all of this
      // is done.
      //
      // We use an AbortController to make it easier to manage the cleanup.
      let controller = new AbortController();
      let { signal } = controller;

      // This nsIObserver will listen for the UIState status to change to
      // STATUS_SIGNED_IN as our definitive signal that FxA sign-in has
      // completed. It will then resolve the outer Promise to `true`.
      let fxaObserver = {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIObserver,
          Ci.nsISupportsWeakReference,
        ]),

        observe(aSubject, aTopic, aData) {
          let state = lazy.UIState.get();
          if (state.status === lazy.UIState.STATUS_SIGNED_IN) {
            // We completed sign-in, so tear down our listener / observer and resolve
            // didSignIn to true.
            controller.abort();
            resolve(true);
          }
        },
      };

      // The TabClose event listener _can_ accept the AbortController signal,
      // which will then remove the event listener after controller.abort is
      // called.
      fxaTab.addEventListener(
        "TabClose",
        () => {
          // If the TabClose event was fired before the event handler was
          // removed, this means that the tab was closed and sign-in was
          // not completed, which means we should resolve didSignIn to false.
          controller.abort();
          resolve(false);
        },
        { once: true, signal }
      );

      let window = fxaTab.ownerGlobal;
      window.addEventListener("unload", () => {
        // If the hosting window unload event was fired before the event handler
        // was removed, this means that the window was closed and sign-in was
        // not completed, which means we should resolve didSignIn to false.
        controller.abort();
        resolve(false);
      });

      Services.obs.addObserver(fxaObserver, lazy.UIState.ON_UPDATE);

      // Unfortunately, nsIObserverService.addObserver does not accept an
      // AbortController signal as a parameter, so instead we listen for the
      // abort event on the signal to remove the observer.
      signal.addEventListener(
        "abort",
        () => {
          Services.obs.removeObserver(fxaObserver, lazy.UIState.ON_UPDATE);
        },
        { once: true }
      );
    });

    // If the user completed sign-in, we'll close the fxaBrowser tab for
    // them to bring them back to the about:welcome flow.
    //
    // If the sign-in page was loaded in a new window, this will close the
    // tab for that window. That will close the window as well if it's the
    // last tab in that window.
    if (didSignIn && data?.autoClose !== false) {
      gBrowser.removeTab(fxaTab);
    }

    return didSignIn;
  },

  /**
   * Processes "Special Message Actions", which are definitions of behaviors such as opening tabs
   * installing add-ons, or focusing the awesome bar that are allowed to can be triggered from
   * Messaging System interactions.
   *
   * @param {{type: string, data?: any}} action User action defined in message JSON.
   * @param browser {Browser} The browser most relevant to the message.
   * @returns {Promise<unknown>} Type depends on action type. See cases below.
   */
  /* eslint-disable-next-line complexity */
  async handleAction(action, browser) {
    const window = browser.ownerGlobal;
    switch (action.type) {
      case "SHOW_MIGRATION_WIZARD":
        Services.tm.dispatchToMainThread(() =>
          lazy.MigrationUtils.showMigrationWizard(window, {
            entrypoint: lazy.MigrationUtils.MIGRATION_ENTRYPOINTS.NEWTAB,
            migratorKey: action.data?.source,
          })
        );
        break;
      case "OPEN_PRIVATE_BROWSER_WINDOW":
        // Forcefully open about:privatebrowsing
        window.OpenBrowserWindow({ private: true });
        break;
      case "OPEN_URL":
        window.openLinkIn(
          Services.urlFormatter.formatURL(action.data.args),
          action.data.where || "current",
          {
            private: false,
            triggeringPrincipal:
              Services.scriptSecurityManager.createNullPrincipal({}),
            csp: null,
          }
        );
        break;
      case "OPEN_ABOUT_PAGE":
        let aboutPageURL = new URL(`about:${action.data.args}`);
        if (action.data.entrypoint) {
          aboutPageURL.search = action.data.entrypoint;
        }
        window.openTrustedLinkIn(
          aboutPageURL.toString(),
          action.data.where || "tab"
        );
        break;
      case "OPEN_FIREFOX_VIEW":
        window.FirefoxViewHandler.openTab();
        break;
      case "OPEN_PREFERENCES_PAGE":
        window.openPreferences(
          action.data.category || action.data.args,
          action.data.entrypoint && {
            urlParams: { entrypoint: action.data.entrypoint },
          }
        );
        break;
      case "OPEN_APPLICATIONS_MENU":
        lazy.UITour.showMenu(window, action.data.args);
        break;
      case "HIGHLIGHT_FEATURE":
        const highlight = await lazy.UITour.getTarget(window, action.data.args);
        if (highlight) {
          await lazy.UITour.showHighlight(window, highlight, "none", {
            autohide: true,
          });
        }
        break;
      case "INSTALL_ADDON_FROM_URL":
        await this.installAddonFromURL(
          browser,
          action.data.url,
          action.data.telemetrySource
        );
        break;
      case "PIN_FIREFOX_TO_TASKBAR":
        await this.pinFirefoxToTaskbar(window, action.data?.privatePin);
        break;
      case "PIN_AND_DEFAULT":
        await this.pinFirefoxToTaskbar(window, action.data?.privatePin);
        await this.setDefaultBrowser(window);
        break;
      case "SET_DEFAULT_BROWSER":
        await this.setDefaultBrowser(window);
        break;
      case "SET_DEFAULT_PDF_HANDLER":
        this.setDefaultPDFHandler(
          window,
          action.data?.onlyIfKnownBrowser ?? false
        );
        break;
      case "DECLINE_DEFAULT_PDF_HANDLER":
        Services.prefs.setBoolPref(
          "browser.shell.checkDefaultPDF.silencedByUser",
          true
        );
        break;
      case "CONFIRM_LAUNCH_ON_LOGIN":
        const { WindowsLaunchOnLogin } = ChromeUtils.importESModule(
          "resource://gre/modules/WindowsLaunchOnLogin.sys.mjs"
        );
        await WindowsLaunchOnLogin.createLaunchOnLoginRegistryKey();
        break;
      case "PIN_CURRENT_TAB":
        let tab = window.gBrowser.selectedTab;
        window.gBrowser.pinTab(tab);
        window.ConfirmationHint.show(tab, "confirmation-hint-pin-tab", {
          descriptionId: "confirmation-hint-pin-tab-description",
        });
        break;
      case "SHOW_FIREFOX_ACCOUNTS":
        if (!(await lazy.FxAccounts.canConnectAccount())) {
          break;
        }
        const data = action.data;
        const url = await lazy.FxAccounts.config.promiseConnectAccountURI(
          data && data.entrypoint,
          (data && data.extraParams) || {}
        );
        // Use location provided; if not specified, replace the current tab.
        window.openLinkIn(url, data.where || "current", {
          private: false,
          triggeringPrincipal:
            Services.scriptSecurityManager.createNullPrincipal({}),
          csp: null,
        });
        break;
      case "FXA_SIGNIN_FLOW":
        /** @returns {Promise<boolean>} */
        return this.fxaSignInFlow(action.data, browser);
      case "OPEN_PROTECTION_PANEL":
        let { gProtectionsHandler } = window;
        gProtectionsHandler.showProtectionsPopup({});
        break;
      case "OPEN_PROTECTION_REPORT":
        window.gProtectionsHandler.openProtections();
        break;
      case "OPEN_AWESOME_BAR":
        window.gURLBar.search("");
        break;
      case "DISABLE_STP_DOORHANGERS":
        await this.blockMessageById([
          "SOCIAL_TRACKING_PROTECTION",
          "FINGERPRINTERS_PROTECTION",
          "CRYPTOMINERS_PROTECTION",
        ]);
        break;
      case "DISABLE_DOH":
        Services.prefs.setStringPref(
          DOH_DOORHANGER_DECISION_PREF,
          "UIDisabled"
        );
        Services.prefs.setIntPref(NETWORK_TRR_MODE_PREF, 5);
        break;
      case "ACCEPT_DOH":
        Services.prefs.setStringPref(DOH_DOORHANGER_DECISION_PREF, "UIOk");
        break;
      case "CANCEL":
        // A no-op used by CFRs that minimizes the notification but does not
        // trigger a dismiss or block (it keeps the notification around)
        break;
      case "CONFIGURE_HOMEPAGE":
        this.configureHomepage(action.data);
        break;
      case "SHOW_SPOTLIGHT":
        lazy.Spotlight.showSpotlightDialog(browser, action.data);
        break;
      case "BLOCK_MESSAGE":
        await this.blockMessageById(action.data.id);
        break;
      case "SET_PREF":
        this.setPref(action.data.pref);
        break;
      case "MULTI_ACTION":
        await Promise.all(
          action.data.actions.map(async action => {
            try {
              await this.handleAction(action, browser);
            } catch (err) {
              throw new Error(`Error in MULTI_ACTION event: ${err.message}`);
            }
          })
        );
        break;
      default:
        throw new Error(
          `Special message action with type ${action.type} is unsupported.`
        );
      case "CLICK_ELEMENT":
        const clickElement = window.document.querySelector(
          action.data.selector
        );
        clickElement?.click();
        break;
      case "RELOAD_BROWSER":
        browser.reload();
        break;
      case "FOCUS_URLBAR":
        window.gURLBar.focus();
        window.gURLBar.select();
        break;
    }
    return undefined;
  },
};
