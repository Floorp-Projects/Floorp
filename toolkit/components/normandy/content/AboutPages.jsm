/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cm = Components.manager;
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(
  this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm",
);
ChromeUtils.defineModuleGetter(
  this, "AddonStudies", "resource://normandy/lib/AddonStudies.jsm",
);
ChromeUtils.defineModuleGetter(
  this, "RecipeRunner", "resource://normandy/lib/RecipeRunner.jsm",
);

var EXPORTED_SYMBOLS = ["AboutPages"];

const SHIELD_LEARN_MORE_URL_PREF = "app.normandy.shieldLearnMoreUrl";

// Due to bug 1051238 frame scripts are cached forever, so we can't update them
// as a restartless add-on. The Math.random() is the work around for this.
const PROCESS_SCRIPT = (
  `resource://normandy-content/shield-content-process.js?${Math.random()}`
);
const FRAME_SCRIPT = (
  `resource://normandy-content/shield-content-frame.js?${Math.random()}`
);

/**
 * Class for managing an about: page that Normandy provides. Adapted from
 * browser/extensions/pocket/content/AboutPocket.jsm.
 *
 * @implements nsIFactory
 * @implements nsIAboutModule
 */
class AboutPage {
  constructor({chromeUrl, aboutHost, classId, description, uriFlags}) {
    this.chromeUrl = chromeUrl;
    this.aboutHost = aboutHost;
    this.classId = Components.ID(classId);
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

  createInstance(outer, iid) {
    if (outer !== null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  }

  /**
   * Register this about: page with XPCOM. This must be called once in each
   * process (parent and content) to correctly initialize the page.
   */
  register() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).registerFactory(
      this.classId,
      this.description,
      `@mozilla.org/network/protocol/about;1?what=${this.aboutHost}`,
      this,
    );
  }

  /**
   * Unregister this about: page with XPCOM. This must be called before the
   * add-on is cleaned up if the page has been registered.
   */
  unregister() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).unregisterFactory(this.classId, this);
  }
}
AboutPage.prototype.QueryInterface = ChromeUtils.generateQI([Ci.nsIAboutModule]);

/**
 * The module exported by this file.
 */
var AboutPages = {
  async init() {
    // Load scripts in content processes and tabs
    Services.ppmm.loadProcessScript(PROCESS_SCRIPT, true);
    Services.mm.loadFrameScript(FRAME_SCRIPT, true);

    // Register about: pages and their listeners
    this.aboutStudies.register();
    this.aboutStudies.registerParentListeners();

    CleanupManager.addCleanupHandler(() => {
      // Stop loading processs scripts and notify existing scripts to clean up.
      Services.ppmm.removeDelayedProcessScript(PROCESS_SCRIPT);
      Services.ppmm.broadcastAsyncMessage("Shield:ShuttingDown");
      Services.mm.removeDelayedFrameScript(FRAME_SCRIPT);
      Services.mm.broadcastAsyncMessage("Shield:ShuttingDown");

      // Clean up about pages
      this.aboutStudies.unregisterParentListeners();
      this.aboutStudies.unregister();
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
    classId: "{6ab96943-a163-482c-9622-4faedc0e827f}",
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
      Services.mm.addMessageListener("Shield:GetStudyList", this);
      Services.mm.addMessageListener("Shield:RemoveStudy", this);
      Services.mm.addMessageListener("Shield:OpenDataPreferences", this);
      Services.mm.addMessageListener("Shield:GetStudiesEnabled", this);
    },

    /**
     * Unregister listeners for messages from the content process.
     */
    unregisterParentListeners() {
      Services.mm.removeMessageListener("Shield:GetStudyList", this);
      Services.mm.removeMessageListener("Shield:RemoveStudy", this);
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
        case "Shield:GetStudyList":
          this.sendStudyList(message.target);
          break;
        case "Shield:RemoveStudy":
          this.removeStudy(message.data.recipeId, message.data.reason);
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
     * Fetch a list of studies from storage and send it to the process that
     * requested them.
     * @param {<browser>} target
     *   XUL <browser> element for the tab containing the about:studies page
     *   that requested a study list.
     */
    async sendStudyList(target) {
      try {
        target.messageManager.sendAsyncMessage("Shield:ReceiveStudyList", {
          studies: await AddonStudies.getAll(),
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
      try {
        target.messageManager.sendAsyncMessage("Shield:ReceiveStudiesEnabled", {
          studiesEnabled: RecipeRunner.enabled,
        });
      } catch (err) {
        // The child process might be gone, so no need to throw here.
        Cu.reportError(err);
      }
    },

    /**
     * Disable an active study and remove its add-on.
     * @param {String} studyName
     */
    async removeStudy(recipeId, reason) {
      await AddonStudies.stop(recipeId, reason);

      // Update any open tabs with the new study list now that it has changed.
      Services.mm.broadcastAsyncMessage("Shield:ReceiveStudyList", {
        studies: await AddonStudies.getAll(),
      });
    },

    openDataPreferences() {
      const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
      browserWindow.openPreferences("privacy-reports", {origin: "aboutStudies"});
    },

    getShieldLearnMoreHref() {
      return Services.urlFormatter.formatURLPref(SHIELD_LEARN_MORE_URL_PREF);
    },

    getStudiesEnabled() {
      RecipeRunner.checkPrefs();
      return RecipeRunner.enabled;
    }
  });

  return aboutStudies;
});
