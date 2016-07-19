/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["ContextualIdentityService"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm")

const DEFAULT_TAB_COLOR = "#909090"

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/browser.properties");
});

this.ContextualIdentityService = {
  _identities: [
    { userContextId: 1,
      public: true,
      icon: "chrome://browser/skin/usercontext/personal.svg",
      color: "#00a7e0",
      label: "userContextPersonal.label",
      accessKey: "userContextPersonal.accesskey",
      alreadyOpened: false },
    { userContextId: 2,
      public: true,
      icon: "chrome://browser/skin/usercontext/work.svg",
      color: "#f89c24",
      label: "userContextWork.label",
      accessKey: "userContextWork.accesskey",
      alreadyOpened: false },
    { userContextId: 3,
      public: true,
      icon: "chrome://browser/skin/usercontext/banking.svg",
      color: "#7dc14c",
      label: "userContextBanking.label",
      accessKey: "userContextBanking.accesskey",
      alreadyOpened: false },
    { userContextId: 4,
      public: true,
      icon: "chrome://browser/skin/usercontext/shopping.svg",
      color: "#ee5195",
      label: "userContextShopping.label",
      accessKey: "userContextShopping.accesskey",
      alreadyOpened: false },
    { userContextId: Math.pow(2, 31) - 1,
      public: false,
      icon: "",
      color: "",
      label: "userContextIdInternal.thumbnail",
      accessKey: "",
      alreadyOpened: false },
  ],

  getIdentities() {
    return this._identities.filter(info => info.public);
  },

  getPrivateIdentity(label) {
    return this._identities.find(info => !info.public && info.label == label);
  },

  getIdentityFromId(userContextId) {
    return this._identities.find(info => info.userContextId == userContextId);
  },

  getUserContextLabel(userContextId) {
    let identity = this.getIdentityFromId(userContextId);
    if (!identity.public) {
      return "";
    }
    return gBrowserBundle.GetStringFromName(identity.label);
  },

  setTabStyle(tab) {
    // inline style is only a temporary fix for some bad performances related
    // to the use of CSS vars. This code will be removed in bug 1278177.
    if (!tab.hasAttribute("usercontextid")) {
      tab.style.removeProperty("background-image");
      tab.style.removeProperty("background-size");
      tab.style.removeProperty("background-repeat");
      return;
    }

    let userContextId = tab.getAttribute("usercontextid");
    let identity = this.getIdentityFromId(userContextId);

    let color = identity ? identity.color : DEFAULT_TAB_COLOR;
    tab.style.backgroundImage = "linear-gradient(to right, transparent 20%, " + color + " 30%, " + color + " 70%, transparent 80%)";
    tab.style.backgroundSize = "auto 2px";
    tab.style.backgroundRepeat = "no-repeat";
  },

  telemetry(userContextId) {
    let identity = this.getIdentityFromId(userContextId);

    // Let's ignore unknown identities for now.
    if (!identity) {
      return;
    }

    if (!identity.alreadyOpened) {
      identity.alreadyOpened = true;
      Services.telemetry.getHistogramById("UNIQUE_CONTAINERS_OPENED").add(1);
    }

    Services.telemetry.getHistogramById("TOTAL_CONTAINERS_OPENED").add(1);
  },
}
