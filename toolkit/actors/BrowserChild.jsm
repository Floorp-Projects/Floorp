/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");

class BrowserChild extends ActorChild {
  handleEvent(event) {
    if (event.type == "DOMWindowClose") {
      this.mm.sendAsyncMessage("DOMWindowClose");
    }
  }

  receiveMessage(message) {
    switch (message.name) {
    case "Browser:CreateAboutBlank":
      if (!this.content.document || this.content.document.documentURI != "about:blank") {
        throw new Error("Can't create a content viewer unless on about:blank");
      }
      let principal = message.data;
      principal = BrowserUtils.principalWithMatchingOA(principal, this.content.document.nodePrincipal);
      this.mm.docShell.createAboutBlankContentViewer(principal);
      break;

    case "InPermitUnload":
      let inPermitUnload = this.mm.docShell.contentViewer && this.mm.docShell.contentViewer.inPermitUnload;
      this.mm.sendAsyncMessage("InPermitUnload", {id: message.data.id, inPermitUnload});
      break;

    case "PermitUnload":
      this.mm.sendAsyncMessage("PermitUnload", {id: message.data.id, kind: "start"});

      let permitUnload = true;
      if (this.mm.docShell && this.mm.docShell.contentViewer) {
        permitUnload = this.mm.docShell.contentViewer.permitUnload(message.data.aPermitUnloadFlags);
      }

      this.mm.sendAsyncMessage("PermitUnload", {id: message.data.id, kind: "end", permitUnload});
      break;
    }
  }
}
