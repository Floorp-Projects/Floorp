/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AddonStudies", "resource://normandy/lib/AddonStudies.jsm");
ChromeUtils.defineModuleGetter(this, "AddonStudyAction", "resource://normandy/actions/AddonStudyAction.jsm");
ChromeUtils.defineModuleGetter(this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm");
ChromeUtils.defineModuleGetter(this, "PreferenceExperiments", "resource://normandy/lib/PreferenceExperiments.jsm");
ChromeUtils.defineModuleGetter(this, "RecipeRunner", "resource://normandy/lib/RecipeRunner.jsm");

var EXPORTED_SYMBOLS = ["AboutPages"];

const SHIELD_LEARN_MORE_URL_PREF = "app.normandy.shieldLearnMoreUrl";
XPCOMUtils.defineLazyPreferenceGetter(this, "gOptOutStudiesEnabled", "app.shield.optoutstudies.enabled");


/**
 * Class for managing an about: page that Normandy provides. Adapted from
 * browser/components/pocket/content/AboutPocket.jsm.
 *
 * @implements nsIFactory
 * @implements nsIAboutModule
 */
class AboutPage {
  constructor({chromeUrl, aboutHost, classID, description, uriFlags}) {
    this.chromeUrl = chromeUrl;
    this.aboutHost = aboutHost;
    this.classID = Components.ID(classID);
    this.description = description;
    this.uriFlags = uriFlags;
  }

  getURIFlags() {
    return this.uriFlags;
  }

  newChannel(uri, loadInfo) {
    const newURI = Services.io.newURI(this.chromeUrl);
    const channel = Services.io.newChannelFromURIWithLoadInfo(newURI, loadInfo);
    channel.originalURI = uri;

    if (this.uriFlags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      const principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
      channel.owner = principal;
    }
    return channel;
  }
}
AboutPage.prototype.QueryInterface = ChromeUtils.generateQI([Ci.nsIAboutModule]);

/**
 * The module exported by this file.
 */
var AboutPages = {
  async init() {
    // Load scripts in content processes and tabs

    // Register about: pages and their listeners
    this.aboutStudies.registerParentListeners();

    CleanupManager.addCleanupHandler(() => {
      // Stop loading process scripts and notify existing scripts to clean up.
      Services.ppmm.broadcastAsyncMessage("Shield:ShuttingDown");
      Services.mm.broadcastAsyncMessage("Shield:ShuttingDown");

      // Clean up about pages
      this.aboutStudies.unregisterParentListeners();
    });
  },
};

/**
 * about:studies page for displaying in-progress and past Shield studies.
 * @type {AboutPage}
 * @implements {nsIMessageListener}
 */
XPCOMUtils.defineLazyGetter(this.AboutPages, "aboutStudies", () => {
  const aboutStudies = new AboutPage({
    chromeUrl: "resource://normandy-content/about-studies/about-studies.html",
    aboutHost: "studies",
    classID: "{6ab96943-a163-482c-9622-4faedc0e827f}",
    description: "Shield Study Listing",
    uriFlags: (
      Ci.nsIAboutModule.ALLOW_SCRIPT
      | Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT
      | Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD
    ),
  });

  // Extra methods for about:study-specific behavior.
  Object.assign(aboutStudies, {
    /**
     * Register listeners for messages from the content processes.
     */
    registerParentListeners() {
      Services.mm.addMessageListener("Shield:GetAddonStudyList", this);
      Services.mm.addMessageListener("Shield:GetPreferenceStudyList", this);
      Services.mm.addMessageListener("Shield:RemoveAddonStudy", this);
      Services.mm.addMessageListener("Shield:RemovePreferenceStudy", this);
      Services.mm.addMessageListener("Shield:OpenDataPreferences", this);
      Services.mm.addMessageListener("Shield:GetStudiesEnabled", this);
    },

    /**
     * Unregister listeners for messages from the content process.
     */
    unregisterParentListeners() {
      Services.mm.removeMessageListener("Shield:GetAddonStudyList", this);
      Services.mm.removeMessageListener("Shield:GetPreferenceStudyList", this);
      Services.mm.removeMessageListener("Shield:RemoveAddonStudy", this);
      Services.mm.removeMessageListener("Shield:RemovePreferenceStudy", this);
      Services.mm.removeMessageListener("Shield:OpenDataPreferences", this);
      Services.mm.removeMessageListener("Shield:GetStudiesEnabled", this);
    },

    /**
     * Dispatch messages from the content process to the appropriate handler.
     * @param {Object} message
     *   See the nsIMessageListener documentation for details about this object.
     */
    receiveMessage(message) {
      switch (message.name) {
        case "Shield:GetAddonStudyList":
          this.sendAddonStudyList(message.target);
          break;
        case "Shield:GetPreferenceStudyList":
          this.sendPreferenceStudyList(message.target);
          break;
        case "Shield:RemoveAddonStudy":
          this.removeAddonStudy(message.data.recipeId, message.data.reason);
          break;
        case "Shield:RemovePreferenceStudy":
          this.removePreferenceStudy(message.data.experimentName, message.data.reason);
          break;
        case "Shield:OpenDataPreferences":
          this.openDataPreferences();
          break;
        case "Shield:GetStudiesEnabled":
          this.sendStudiesEnabled(message.target);
          break;
      }
    },

    /**
     * Fetch a list of add-on studies from storage and send it to the process
     * that requested them.
     * @param {<browser>} target
     *   XUL <browser> element for the tab containing the about:studies page
     *   that requested a study list.
     */
    async sendAddonStudyList(target) {
      try {
        target.messageManager.sendAsyncMessage("Shield:ReceiveAddonStudyList", {
          studies: await AddonStudies.getAll(),
        });
      } catch (err) {
        // The child process might be gone, so no need to throw here.
        Cu.reportError(err);
      }
    },

    /**
     * Fetch a list of preference studies from storage and send it to the
     * process that requested them.
     * @param {<browser>} target
     *   XUL <browser> element for the tab containing the about:studies page
     *   that requested a study list.
     */
    async sendPreferenceStudyList(target) {
      try {
        target.messageManager.sendAsyncMessage("Shield:ReceivePreferenceStudyList", {
          studies: await PreferenceExperiments.getAll(),
        });
      } catch (err) {
        // The child process might be gone, so no need to throw here.
        Cu.reportError(err);
      }
    },

    /**
     * Get if studies are enabled and send it to the process that
     * requested them. This has to be in the parent process, since
     * RecipeRunner is stateful, and can't be interacted with from
     * content processes safely.
     *
     * @param {<browser>} target
     *   XUL <browser> element for the tab containing the about:studies page
     *   that requested a study list.
     */
    sendStudiesEnabled(target) {
      RecipeRunner.checkPrefs();
      const studiesEnabled = RecipeRunner.enabled && gOptOutStudiesEnabled;
      try {
        target.messageManager.sendAsyncMessage("Shield:ReceiveStudiesEnabled", { studiesEnabled });
      } catch (err) {
        // The child process might be gone, so no need to throw here.
        Cu.reportError(err);
      }
    },

    /**
     * Disable an active add-on study and remove its add-on.
     * @param {String} studyName
     */
    async removeAddonStudy(recipeId, reason) {
      const action = new AddonStudyAction();
      await action.unenroll(recipeId, reason);

      // Update any open tabs with the new study list now that it has changed.
      Services.mm.broadcastAsyncMessage("Shield:ReceiveAddonStudyList", {
        studies: await AddonStudies.getAll(),
      });
    },

    /**
     * Disable an active preference study
     * @param {String} studyName
     */
    async removePreferenceStudy(experimentName, reason) {
      PreferenceExperiments.stop(experimentName, { reason });

      // Update any open tabs with the new study list now that it has changed.
      Services.mm.broadcastAsyncMessage("Shield:ReceivePreferenceStudyList", {
        studies: await PreferenceExperiments.getAll(),
      });
    },

    openDataPreferences() {
      const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
      browserWindow.openPreferences("privacy-reports");
    },

    getShieldLearnMoreHref() {
      return Services.urlFormatter.formatURLPref(SHIELD_LEARN_MORE_URL_PREF);
    },
  });

  return aboutStudies;
});
