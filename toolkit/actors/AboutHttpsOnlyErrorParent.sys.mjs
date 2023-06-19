/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { HomePage } from "resource:///modules/HomePage.sys.mjs";
import { PrivateBrowsingUtils } from "resource://gre/modules/PrivateBrowsingUtils.sys.mjs";

export class AboutHttpsOnlyErrorParent extends JSWindowActorParent {
  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "goBack":
        this.goBackFromErrorPage(this.browser);
        break;
    }
  }

  goBackFromErrorPage(aBrowser) {
    if (!aBrowser.canGoBack) {
      // If the unsafe page is the first or the only one in history, go to the
      // start page.
      aBrowser.fixupAndLoadURIString(
        this.getDefaultHomePage(aBrowser.ownerGlobal),
        {
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
        }
      );
    } else {
      aBrowser.goBack();
    }
  }

  getDefaultHomePage(win) {
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return win.BROWSER_NEW_TAB_URL || "about:blank";
    }
    let url = HomePage.getDefault();
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.includes("|")) {
      url = url.split("|")[0];
    }
    return url;
  }
}
