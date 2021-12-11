/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["SpecialMessageActions"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const DOH_DOORHANGER_DECISION_PREF = "doh-rollout.doorhanger-decision";
const NETWORK_TRR_MODE_PREF = "network.trr.mode";

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  UITour: "resource:///modules/UITour.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
});

const SpecialMessageActions = {
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
      const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

      // AddonManager installation source associated to the addons installed from activitystream's CFR
      // and RTAMO (source is going to be "amo" if not configured explicitly in the message provider).
      const telemetryInfo = { source: telemetrySource };
      const install = await AddonManager.getInstallForURL(aUri.spec, {
        telemetryInfo,
      });
      await AddonManager.installAddonFromWebpage(
        "application/x-xpinstall",
        browser,
        systemPrincipal,
        install
      );
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Pin Firefox to taskbar.
   *
   * @param {Window} window Reference to a window object
   */
  pinFirefoxToTaskbar(window) {
    window.getShellService().pinToTaskbar();
  },

  /**
   *  Set browser as the operating system default browser.
   *
   *  @param {Window} window Reference to a window object
   */
  setDefaultBrowser(window) {
    window.getShellService().setAsDefault();
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
          // controls the snippets section
          "browser.newtabpage.activity-stream.feeds.snippets",
          layout.snippets,
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
   * Processes "Special Message Actions", which are definitions of behaviors such as opening tabs
   * installing add-ons, or focusing the awesome bar that are allowed to can be triggered from
   * Messaging System interactions.
   *
   * @param {{type: string, data?: any}} action User action defined in message JSON.
   * @param browser {Browser} The browser most relvant to the message.
   */
  async handleAction(action, browser) {
    const window = browser.ownerGlobal;
    switch (action.type) {
      case "SHOW_MIGRATION_WIZARD":
        MigrationUtils.showMigrationWizard(window, [
          MigrationUtils.MIGRATION_ENTRYPOINT_NEWTAB,
          action.data?.source,
        ]);
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
            triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
              {}
            ),
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
      case "OPEN_PREFERENCES_PAGE":
        window.openPreferences(
          action.data.category || action.data.args,
          action.data.entrypoint && {
            urlParams: { entrypoint: action.data.entrypoint },
          }
        );
        break;
      case "OPEN_APPLICATIONS_MENU":
        UITour.showMenu(window, action.data.args);
        break;
      case "HIGHLIGHT_FEATURE":
        const highlight = await UITour.getTarget(window, action.data.args);
        if (highlight) {
          await UITour.showHighlight(window, highlight, "none", {
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
        this.pinFirefoxToTaskbar(window);
        break;
      case "PIN_AND_DEFAULT":
        this.pinFirefoxToTaskbar(window);
        this.setDefaultBrowser(window);
        break;
      case "SET_DEFAULT_BROWSER":
        this.setDefaultBrowser(window);
        break;
      case "PIN_CURRENT_TAB":
        let tab = window.gBrowser.selectedTab;
        window.gBrowser.pinTab(tab);
        window.ConfirmationHint.show(tab, "pinTab", {
          showDescription: true,
        });
        break;
      case "SHOW_FIREFOX_ACCOUNTS":
        const data = action.data;
        const url = await FxAccounts.config.promiseConnectAccountURI(
          (data && data.entrypoint) || "snippets",
          (data && data.extraParams) || {}
        );
        // We want to replace the current tab.
        window.openLinkIn(url, "current", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
            {}
          ),
          csp: null,
        });
        break;
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
        const topWindow = browser.ownerGlobal.window.BrowserWindowTracker.getTopWindow();
        if (topWindow) {
          topWindow.BrowserHome();
        }
        break;
      case "ENABLE_TOTAL_COOKIE_PROTECTION":
        Services.prefs.setBoolPref(
          "privacy.restrict3rdpartystorage.rollout.enabledByDefault",
          true
        );
        break;
      case "ENABLE_TOTAL_COOKIE_PROTECTION_SECTION_AND_OPT_OUT":
        Services.prefs.setBoolPref(
          "privacy.restrict3rdpartystorage.rollout.enabledByDefault",
          false
        );
        Services.prefs.setBoolPref(
          "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard",
          true
        );
        break;
      default:
        throw new Error(
          `Special message action with type ${action.type} is unsupported.`
        );
    }
  },
};
