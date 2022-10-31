/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1760754 - Shim Nielsen tracker
 *
 * Sites expecting the Nielsen tracker to load properly can break if it
 * is blocked. This shim mitigates that breakage by loading a stand-in.
 */

if (!window.nol_t) {
  const cid = "";

  let domain = "";
  let schemeHost = "";
  let scriptName = "";
  try {
    const url = document?.currentScript?.src;
    const { pathname, protocol, host } = new URL(url);
    domain = host
      .split(".")
      .slice(0, -2)
      .join(".");
    schemeHost = `${protocol}//${host}/`;
    scriptName = pathname.split("/").pop();
  } catch (_) {}

  const NolTracker = class {
    CONST = {
      max_tags: 20,
    };
    feat = {};
    globals = {
      cid,
      content: "0",
      defaultApidFile: "config250",
      defaultErrorParams: {
        nol_vcid: "c00",
        nol_clientid: "",
      },
      domain,
      fpidSfCodeList: [""],
      init() {},
      tagCurrRetry: -1,
      tagMaxRetry: 3,
      wlCurrRetry: -1,
      wlMaxRetry: 3,
    };
    pmap = [];
    pvar = {
      cid,
      content: "0",
      cookies_enabled: "n",
      server: domain,
    };
    scriptName = [scriptName];
    version = "6.0.107";

    addScript() {}
    catchLinkOverlay() {}
    clickEvent() {}
    clickTrack() {}
    do_sample() {}
    downloadEvent() {}
    eventTrack() {}
    filter() {}
    fireToUrl() {}
    getSchemeHost() {
      return schemeHost;
    }
    getVersion() {}
    iframe() {}
    in_sample() {
      return true;
    }
    injectBsdk() {}
    invite() {}
    linkTrack() {}
    mergeFeatures() {}
    pageEvent() {}
    pause() {}
    populateWhitelist() {}
    post() {}
    postClickTrack() {}
    postData() {}
    postEvent() {}
    postEventTrack() {}
    postLinkTrack() {}
    prefix() {
      return "";
    }
    processDdrsSvc() {}
    random() {}
    record() {
      return this;
    }
    regLinkOverlay() {}
    regListen() {}
    retrieveCiFileViaCors() {}
    sectionEvent() {}
    sendALink() {}
    sendForm() {}
    sendIt() {}
    slideEvent() {}
    whitelistAssigned() {}
  };

  window.nol_t = () => {
    return new NolTracker();
  };
}
