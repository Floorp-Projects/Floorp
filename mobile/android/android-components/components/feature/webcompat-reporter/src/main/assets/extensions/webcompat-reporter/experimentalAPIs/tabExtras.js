/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI */

const lazy = {};

const DEFAULT_NEW_REPORT_ENDPOINT = "https://webcompat.com/issues/new";
const NEW_REPORT_ENDPOINT_PREF =
  "ui.new-webcompat-reporter.new-report-endpoint";

this.tabExtras = class extends ExtensionAPI {
  getAPI(context) {
    const { tabManager } = context.extension;
    const queryReportBrokenSiteActor = (tabId, name, params) => {
      const { browser } = tabManager.get(tabId);
      const windowGlobal = browser.browsingContext.currentWindowGlobal;
      if (!windowGlobal) {
        return null;
      }
      return windowGlobal.getActor("ReportBrokenSite").sendQuery(name, params);
    };
    return {
      tabExtras: {
        async getWebcompatInfo(tabId) {
          const endpointUrl = Services.prefs.getStringPref(
            NEW_REPORT_ENDPOINT_PREF,
            DEFAULT_NEW_REPORT_ENDPOINT
          );
          const webcompatInfo = await queryReportBrokenSiteActor(
            tabId,
            "GetWebCompatInfo"
          );
          return {
            webcompatInfo,
            endpointUrl,
          };
        },
        async sendWebcompatInfo(tabId, info) {
          console.error(info);
          return queryReportBrokenSiteActor(
            tabId,
            "SendDataToWebcompatCom",
            info
          );
        },
      },
    };
  }
};
