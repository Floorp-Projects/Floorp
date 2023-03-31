/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class PrintingParent extends JSWindowActorParent {
  receiveMessage(message) {
    let browser = this.browsingContext.top.embedderElement;

    if (message.name == "Printing:Error") {
      browser.ownerGlobal.PrintUtils._displayPrintingError(
        message.data.nsresult,
        message.data.isPrinting,
        browser
      );
    } else if (message.name == "Printing:Preview:CurrentPage") {
      browser.setAttribute("current-page", message.data.currentPage);
    }

    return undefined;
  }
}
