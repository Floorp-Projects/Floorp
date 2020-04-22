/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SidebarSearchParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "promptModalType",
  "prompts.modalType.searchService",
  Services.prompt.MODAL_TYPE_WINDOW
);

class SidebarSearchParent extends JSWindowActorParent {
  receiveMessage(msg) {
    if (msg.name == "Search:AddEngine") {
      return this.addSearchEngine(msg.data);
    }
    return Promise.reject("Unsupported message");
  }

  // Called when a webpage calls window.external.AddSearchProvider
  async addSearchEngine({ pageURL, engineURL }) {
    pageURL = NetUtil.newURI(pageURL);
    engineURL = NetUtil.newURI(engineURL, null, pageURL);

    let iconURL;
    let browser = this.browsingContext.top.embedderElement;
    if (browser.outerBrowser) {
      browser = browser.outerBrowser; // handle RDM mode
    }

    if (browser.mIconURL && browser.mIconURL.startsWith("data:")) {
      iconURL = NetUtil.newURI(browser.mIconURL);
    }

    try {
      // Make sure the URLs are HTTP, HTTPS, or FTP.
      let isWeb = ["https", "http", "ftp"];

      if (!isWeb.includes(engineURL.scheme)) {
        throw new Error("Unsupported search engine URL: " + engineURL.spec);
      }

      if (
        Services.policies &&
        !Services.policies.isAllowed("installSearchEngine")
      ) {
        throw new Error(
          "Search Engine installation blocked by the Enterprise Policy Manager."
        );
      }
    } catch (ex) {
      Cu.reportError(
        "Invalid argument passed to window.external.AddSearchProvider: " + ex
      );

      var searchBundle = Services.strings.createBundle(
        "chrome://global/locale/search/search.properties"
      );
      var brandBundle = Services.strings.createBundle(
        "chrome://branding/locale/brand.properties"
      );
      var brandName = brandBundle.GetStringFromName("brandShortName");
      var title = searchBundle.GetStringFromName("error_invalid_format_title");
      var msg = searchBundle.formatStringFromName("error_invalid_engine_msg2", [
        brandName,
        engineURL.spec,
      ]);
      await Services.prompt.asyncAlert(
        this.browsingContext,
        promptModalType,
        title,
        msg
      );
      return undefined;
    }

    return Services.search
      .addEngine(
        engineURL.spec,
        iconURL ? iconURL.spec : null,
        true,
        null,
        this.browsingContext
      )
      .catch(ex =>
        Cu.reportError(
          "Unable to add search engine to the search service: " + ex
        )
      );
  }
}
