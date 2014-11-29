/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
const Services = require("Services");
const { Cc, Ci } = require("chrome");

Object.defineProperty(this, "addonManager", {
  get: (function () {
    let cached;
    return () => {
      if (cached === undefined) {
        // catch errors as the addonManager might not exist in this environment
        // (eg, xpcshell)
        try {
          cached = Cc["@mozilla.org/addons/integration;1"]
                      .getService(Ci.amIAddonManager);
        } catch (ex) {
          cached = null;
        }
      }
      return cached;
    }
  }())
});

const B2G_ID = "{3c2e2abc-06d4-11e1-ac3b-374f68613e61}";

/**
 * This is a wrapper around amIAddonManager.mapURIToAddonID which always returns
 * false on B2G to avoid loading the add-on manager there and reports any
 * exceptions rather than throwing so that the caller doesn't have to worry
 * about them.
 */
module.exports = function mapURIToAddonID(uri, id) {
  if (!Services.appinfo
      || Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT
      || Services.appinfo.ID == B2G_ID
      || !uri
      || !addonManager) {
    return false;
  }

  try {
    return addonManager.mapURIToAddonID(uri, id);
  }
  catch (e) {
    DevToolsUtils.reportException("mapURIToAddonID", e);
    return false;
  }
};
