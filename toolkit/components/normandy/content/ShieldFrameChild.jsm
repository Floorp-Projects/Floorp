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
 *
 * This file is loaded as a frame script. It will be loaded once per tab that
 * is opened.
 */

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const frameGlobal = {};
ChromeUtils.defineModuleGetter(
  frameGlobal, "AboutPages", "resource://normandy-content/AboutPages.jsm",
);

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.createBundle("chrome://global/locale/aboutStudies.properties");
});

/**
 * Handles incoming events from the parent process and about:studies.
 * @implements nsIMessageListener
 * @implements EventListener
 */
class ShieldFrameChild extends ActorChild {
  handleEvent(event) {
    // We waited until after we received an event to register message listeners
    // in order to save resources for tabs that don't ever load about:studies.
    this.mm.addMessageListener("Shield:ShuttingDown", this);
    this.mm.addMessageListener("Shield:ReceiveStudyList", this);
    this.mm.addMessageListener("Shield:ReceiveStudiesEnabled", this);

    switch (event.detail.action) {
      // Actions that require the parent process
      case "GetRemoteValue:StudyList":
        this.mm.sendAsyncMessage("Shield:GetStudyList");
        break;
      case "RemoveStudy":
        this.mm.sendAsyncMessage("Shield:RemoveStudy", event.detail.data);
        break;
      case "GetRemoteValue:StudiesEnabled":
        this.mm.sendAsyncMessage("Shield:GetStudiesEnabled");
        break;
      case "NavigateToDataPreferences":
        this.mm.sendAsyncMessage("Shield:OpenDataPreferences");
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
        strings.enabledList = gStringBundle.formatStringFromName("enabledList", [brandName], 1);

        this.triggerPageCallback(
          "ReceiveRemoteValue:ShieldTranslations",
           strings
        );
        break;
    }
  }

  /**
   * Handle messages from the parent process.
   * @param {Object} message
   *   See the nsIMessageListener docs.
   */
  receiveMessage(message) {
    switch (message.name) {
      case "Shield:ReceiveStudyList":
        this.triggerPageCallback("ReceiveRemoteValue:StudyList", message.data.studies);
        break;
      case "Shield:ReceiveStudiesEnabled":
        this.triggerPageCallback("ReceiveRemoteValue:StudiesEnabled", message.data.studiesEnabled);
        break;
      case "Shield:ShuttingDown":
        this.onShutdown();
        break;
    }
  }

  /**
   * Trigger an event to communicate with the unprivileged about: page.
   * @param {String} type
   * @param {Object} detail
   */
  triggerPageCallback(type, detail) {
    let {content} = this.mm;

    // Clone details and use the event class from the unprivileged context.
    const event = new content.document.defaultView.CustomEvent(type, {
      bubbles: true,
      detail: Cu.cloneInto(detail, content.document.defaultView),
    });
    content.document.dispatchEvent(event);
  }

  onShutdown() {
    this.mm.removeMessageListener("Shield:SendStudyList", this);
    this.mm.removeMessageListener("Shield:ShuttingDown", this);
    this.mm.removeEventListener("Shield", this);
  }
}
