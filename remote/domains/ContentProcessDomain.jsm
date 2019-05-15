/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentProcessDomain"];

const {Domain} = ChromeUtils.import("chrome://remote/content/domains/Domain.jsm");

ChromeUtils.defineModuleGetter(this, "ContextObserver",
                               "chrome://remote/content/domains/ContextObserver.jsm");

class ContentProcessDomain extends Domain {
  destructor() {
    super.destructor();

    if (this._contextObserver) {
      this._contextObserver.destructor();
    }
  }

  // helpers

  get content() {
    return this.session.content;
  }

  get docShell() {
    return this.session.docShell;
  }

  get chromeEventHandler() {
    return this.docShell.chromeEventHandler;
  }

  get contextObserver() {
    if (!this._contextObserver) {
      this._contextObserver = new ContextObserver(this.chromeEventHandler);
    }
    return this._contextObserver;
  }
}
