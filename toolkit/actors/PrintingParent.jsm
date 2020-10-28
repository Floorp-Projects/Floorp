/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PrintingParent"];

let gTestListener = null;

class PrintingParent extends JSWindowActorParent {
  static setTestListener(listener) {
    gTestListener = listener;
  }

  getPrintPreviewToolbar(browser) {
    return browser.ownerDocument.getElementById("print-preview-toolbar");
  }

  receiveMessage(message) {
    let browser = this.browsingContext.top.embedderElement;
    let PrintUtils = browser.ownerGlobal.PrintUtils;

    if (message.name == "Printing:Error") {
      PrintUtils._displayPrintingError(
        message.data.nsresult,
        message.data.isPrinting
      );
      return undefined;
    }

    if (this.ignoreListeners) {
      return undefined;
    }

    let listener = PrintUtils._webProgressPP.value;
    let data = message.data;

    switch (message.name) {
      case "Printing:Preview:Entered": {
        // This message is sent by the content process once it has completed
        // putting the content into print preview mode. We must wait for that to
        // to complete before switching the chrome UI to print preview mode,
        // otherwise we have layout issues.

        if (gTestListener) {
          gTestListener(browser);
        }

        PrintUtils.printPreviewEntered(browser, message.data);
        break;
      }

      case "Printing:Preview:ReaderModeReady": {
        PrintUtils.readerModeReady(browser);
        break;
      }

      case "Printing:Preview:UpdatePageCount": {
        let toolbar = this.getPrintPreviewToolbar(browser);
        toolbar.updatePageCount(message.data.totalPages);
        break;
      }

      case "Printing:Preview:ProgressChange": {
        if (!PrintUtils._webProgressPP.value) {
          // We somehow didn't get a nsIWebProgressListener to be updated...
          // I guess there's nothing to do.
          return undefined;
        }

        return listener.onProgressChange(
          null,
          null,
          data.curSelfProgress,
          data.maxSelfProgress,
          data.curTotalProgress,
          data.maxTotalProgress
        );
      }

      case "Printing:Preview:StateChange": {
        if (!PrintUtils._webProgressPP.value) {
          // We somehow didn't get a nsIWebProgressListener to be updated...
          // I guess there's nothing to do.
          return undefined;
        }

        if (data.stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          // Strangely, the printing engine sends 2 STATE_STOP messages when
          // print preview is finishing. One has the STATE_IS_DOCUMENT flag,
          // the other has the STATE_IS_NETWORK flag. However, the webProgressPP
          // listener stops listening once the first STATE_STOP is sent.
          // Any subsequent messages result in NS_ERROR_FAILURE errors getting
          // thrown. This should all get torn out once bug 1088061 is fixed.

          // Enable toobar elements that we disabled during update.
          let printPreviewTB = this.getPrintPreviewToolbar(browser);
          printPreviewTB.disableUpdateTriggers(false);
        }

        return listener.onStateChange(null, null, data.stateFlags, data.status);
      }
    }

    return undefined;
  }
}
