/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ShieldFrameChild"];

/**
 * Listen for DOM events bubbling up from the about:studies page, and perform
 * privileged actions in response to them. If we need to do anything that the
 * content process can't handle (such as reading IndexedDB), we send a message
 * to the parent process and handle it there.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const frameGlobal = {};
ChromeUtils.defineModuleGetter(
  frameGlobal,
  "AboutPages",
  "resource://normandy-content/AboutPages.jsm"
);

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.createBundle(
    "chrome://global/locale/aboutStudies.properties"
  );
});

/**
 * Listen for DOM events bubbling up from the about:studies page, and perform
 * privileged actions in response to them. If we need to do anything that the
 * content process can't handle (such as reading IndexedDB), we send a message
 * to the parent process and handle it there.
 */
class ShieldFrameChild extends JSWindowActorChild {
  async handleEvent(event) {
    // On page show or page hide,
    // add this child to the WeakSet in AboutStudies.
    switch (event.type) {
      case "pageshow":
        this.sendAsyncMessage("Shield:AddToWeakSet");
        return;

      case "pagehide":
        this.sendAsyncMessage("Shield:RemoveFromWeakSet");
        return;
    }
    switch (event.detail.action) {
      // Actions that require the parent process
      case "GetRemoteValue:AddonStudyList":
        let addonStudies = await this.sendQuery("Shield:GetAddonStudyList");
        this.triggerPageCallback(
          "ReceiveRemoteValue:AddonStudyList",
          addonStudies
        );
        break;
      case "GetRemoteValue:PreferenceStudyList":
        let prefStudies = await this.sendQuery("Shield:GetPreferenceStudyList");
        this.triggerPageCallback(
          "ReceiveRemoteValue:PreferenceStudyList",
          prefStudies
        );
        break;
      case "GetRemoteValue:MessagingSystemList":
        let experiments = await this.sendQuery("Shield:GetMessagingSystemList");
        this.triggerPageCallback(
          "ReceiveRemoteValue:MessagingSystemList",
          experiments
        );
        break;
      case "RemoveAddonStudy":
        this.sendAsyncMessage("Shield:RemoveAddonStudy", event.detail.data);
        break;
      case "RemovePreferenceStudy":
        this.sendAsyncMessage(
          "Shield:RemovePreferenceStudy",
          event.detail.data
        );
        break;
      case "RemoveMessagingSystemExperiment":
        this.sendAsyncMessage(
          "Shield:RemoveMessagingSystemExperiment",
          event.detail.data
        );
        break;
      case "GetRemoteValue:StudiesEnabled":
        let studiesEnabled = await this.sendQuery("Shield:GetStudiesEnabled");
        this.triggerPageCallback(
          "ReceiveRemoteValue:StudiesEnabled",
          studiesEnabled
        );
        break;
      case "NavigateToDataPreferences":
        this.sendAsyncMessage("Shield:OpenDataPreferences");
        break;
      // Actions that can be performed in the content process
      case "GetRemoteValue:ShieldLearnMoreHref":
        this.triggerPageCallback(
          "ReceiveRemoteValue:ShieldLearnMoreHref",
          frameGlobal.AboutPages.aboutStudies.getShieldLearnMoreHref()
        );
        break;
      case "GetRemoteValue:ShieldTranslations":
        const strings = {};
        for (let str of gStringBundle.getSimpleEnumeration()) {
          strings[str.key] = str.value;
        }
        const brandName = gBrandBundle.GetStringFromName("brandShortName");
        strings.enabledList = gStringBundle.formatStringFromName(
          "enabledList",
          [brandName]
        );

        this.triggerPageCallback(
          "ReceiveRemoteValue:ShieldTranslations",
          strings
        );
        break;
      case "ExperimentOptIn":
        this.sendQuery("Shield:ExperimentOptIn", event.detail.data);
        break;
    }
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "Shield:UpdateAddonStudyList":
        this.triggerPageCallback("ReceiveRemoteValue:AddonStudyList", msg.data);
        break;
      case "Shield:UpdatePreferenceStudyList":
        this.triggerPageCallback(
          "ReceiveRemoteValue:PreferenceStudyList",
          msg.data
        );
        break;
      case "Shield:UpdateMessagingSystemExperimentList":
        this.triggerPageCallback(
          "ReceiveRemoteValue:MessagingSystemList",
          msg.data
        );
        break;
    }
  }
  /**
   * Trigger an event to communicate with the unprivileged about:studies page.
   * @param {String} type The type of event to trigger.
   * @param {Object} detail The data to pass along to the event.
   */
  triggerPageCallback(type, detail) {
    // Clone details and use the event class from the unprivileged context.
    const event = new this.document.defaultView.CustomEvent(type, {
      bubbles: true,
      detail: Cu.cloneInto(detail, this.document.defaultView),
    });
    this.document.dispatchEvent(event);
  }
}
