/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AddonStudies",
  "resource://normandy/lib/AddonStudies.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BranchedAddonStudyAction",
  "resource://normandy/actions/BranchedAddonStudyAction.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PreferenceExperiments",
  "resource://normandy/lib/PreferenceExperiments.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "RecipeRunner",
  "resource://normandy/lib/RecipeRunner.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExperimentManager",
  "resource://messaging-system/experiments/ExperimentManager.jsm"
);

var EXPORTED_SYMBOLS = ["AboutPages"];

const SHIELD_LEARN_MORE_URL_PREF = "app.normandy.shieldLearnMoreUrl";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gOptOutStudiesEnabled",
  "app.shield.optoutstudies.enabled"
);

/**
 * Class for managing an about: page that Normandy provides. Adapted from
 * browser/components/pocket/content/AboutPocket.jsm.
 *
 * @implements nsIFactory
 * @implements nsIAboutModule
 */
class AboutPage {
  constructor({ chromeUrl, aboutHost, classID, description, uriFlags }) {
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
      const principal = Services.scriptSecurityManager.createContentPrincipal(
        uri,
        {}
      );
      channel.owner = principal;
    }
    return channel;
  }
}
AboutPage.prototype.QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);

/**
 * The module exported by this file.
 */
let AboutPages = {};
/**
 * The weak set that keeps track of which browsing contexts
 * have an about:studies page.
 */
let BrowsingContexts = new WeakSet();
/**
 * about:studies page for displaying in-progress and past Shield studies.
 * @type {AboutPage}
 * @implements {nsIMessageListener}
 */
XPCOMUtils.defineLazyGetter(AboutPages, "aboutStudies", () => {
  const aboutStudies = new AboutPage({
    chromeUrl: "resource://normandy-content/about-studies/about-studies.html",
    aboutHost: "studies",
    classID: "{6ab96943-a163-482c-9622-4faedc0e827f}",
    description: "Shield Study Listing",
    uriFlags:
      Ci.nsIAboutModule.ALLOW_SCRIPT |
      Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
      Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD,
  });

  // Extra methods for about:study-specific behavior.
  Object.assign(aboutStudies, {
    getAddonStudyList() {
      return AddonStudies.getAll();
    },

    getPreferenceStudyList() {
      return PreferenceExperiments.getAll();
    },

    getMessagingSystemList() {
      return ExperimentManager.store.getAll();
    },

    /** Add a browsing context to the weak set;
     * this weak set keeps track of all contexts
     * that are housing an about:studies page.
     */
    addToWeakSet(browsingContext) {
      BrowsingContexts.add(browsingContext);
    },
    /** Remove a browsing context to the weak set;
     * this weak set keeps track of all contexts
     * that are housing an about:studies page.
     */
    removeFromWeakSet(browsingContext) {
      BrowsingContexts.delete(browsingContext);
    },

    /**
     * Sends a message to every about:studies page,
     * by iterating over the BrowsingContexts weakset.
     * @param {string} message The message string to send to.
     * @param {object} data The data object to send.
     */
    _sendToAll(message, data) {
      ChromeUtils.nondeterministicGetWeakSetKeys(
        BrowsingContexts
      ).forEach(browser =>
        browser.currentWindowGlobal
          .getActor("ShieldFrame")
          .sendAsyncMessage(message, data)
      );
    },

    /**
     * Get if studies are enabled. This has to be in the parent process,
     * since RecipeRunner is stateful, and can't be interacted with from
     * content processes safely.
     */
    getStudiesEnabled() {
      return RecipeRunner.enabled && gOptOutStudiesEnabled;
    },

    /**
     * Disable an active add-on study and remove its add-on.
     * @param {String} recipeId the id of the addon to remove
     * @param {String} reason the reason for removal
     */
    async removeAddonStudy(recipeId, reason) {
      try {
        const action = new BranchedAddonStudyAction();
        await action.unenroll(recipeId, reason);
      } catch (err) {
        // If the exception was that the study was already removed, that's ok.
        // If not, rethrow the error.
        if (!err.toString().includes("already inactive")) {
          throw err;
        }
      } finally {
        // Update any open tabs with the new study list now that it has changed,
        // even if the above failed.
        this.getAddonStudyList().then(list =>
          this._sendToAll("Shield:UpdateAddonStudyList", list)
        );
      }
    },

    /**
     * Disable an active preference study.
     * @param {String} experimentName the name of the experiment to remove
     * @param {String} reason the reason for removal
     */
    async removePreferenceStudy(experimentName, reason) {
      try {
        await PreferenceExperiments.stop(experimentName, { reason });
      } catch (err) {
        // If the exception was that the study was already removed, that's ok.
        // If not, rethrow the error.
        if (!err.toString().includes("already expired")) {
          throw err;
        }
      } finally {
        // Update any open tabs with the new study list now that it has changed,
        // even if the above failed.
        this.getPreferenceStudyList().then(list =>
          this._sendToAll("Shield:UpdatePreferenceStudyList", list)
        );
      }
    },

    async removeMessagingSystemExperiment(slug, reason) {
      ExperimentManager.unenroll(slug, reason);
      this._sendToAll(
        "Shield:UpdateMessagingSystemExperimentList",
        ExperimentManager.store.getAll()
      );
    },

    openDataPreferences() {
      const browserWindow = Services.wm.getMostRecentWindow(
        "navigator:browser"
      );
      browserWindow.openPreferences("privacy-reports");
    },

    getShieldLearnMoreHref() {
      return Services.urlFormatter.formatURLPref(SHIELD_LEARN_MORE_URL_PREF);
    },
  });

  return aboutStudies;
});
