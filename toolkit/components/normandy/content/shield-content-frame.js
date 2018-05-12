/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Listen for DOM events bubbling up from the about:studies page, and perform
 * privileged actions in response to them. If we need to do anything that the
 * content process can't handle (such as reading IndexedDB), we send a message
 * to the parent process and handle it there.
 *
 * This file is loaded as a frame script. It will be loaded once per tab that
 * is opened.
 */

/* global content addMessageListener removeMessageListener sendAsyncMessage */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

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
class ShieldFrameListener {
  handleEvent(event) {
    // Abort if the current page isn't about:studies.
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    // We waited until after we received an event to register message listeners
    // in order to save resources for tabs that don't ever load about:studies.
    addMessageListener("Shield:ShuttingDown", this);
    addMessageListener("Shield:ReceiveStudyList", this);
    addMessageListener("Shield:ReceiveStudiesEnabled", this);

    switch (event.detail.action) {
      // Actions that require the parent process
      case "GetRemoteValue:StudyList":
        sendAsyncMessage("Shield:GetStudyList");
        break;
      case "RemoveStudy":
        sendAsyncMessage("Shield:RemoveStudy", event.detail.data);
        break;
      case "GetRemoteValue:StudiesEnabled":
        sendAsyncMessage("Shield:GetStudiesEnabled");
        break;
      case "NavigateToDataPreferences":
        sendAsyncMessage("Shield:OpenDataPreferences");
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
        const e = gStringBundle.getSimpleEnumeration();
        while (e.hasMoreElements()) {
          var str = e.getNext().QueryInterface(Ci.nsIPropertyElement);
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
   * Check that the current webpage's origin is about:studies.
   * @return {Boolean}
   */
  ensureTrustedOrigin() {
    return content.document.documentURI.startsWith("about:studies");
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
    // Do not communicate with untrusted pages.
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    // Clone details and use the event class from the unprivileged context.
    const event = new content.document.defaultView.CustomEvent(type, {
      bubbles: true,
      detail: Cu.cloneInto(detail, content.document.defaultView),
    });
    content.document.dispatchEvent(event);
  }

  onShutdown() {
    removeMessageListener("Shield:SendStudyList", this);
    removeMessageListener("Shield:ShuttingDown", this);
    removeEventListener("Shield", this);
  }
}

addEventListener("ShieldPageEvent", new ShieldFrameListener(), false, true);
