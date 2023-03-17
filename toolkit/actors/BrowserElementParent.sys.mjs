/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The BrowserElementParent is for performing actions on one or more subframes of
 * a <xul:browser> from the browser element binding.
 */
export class BrowserElementParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "DOMWindowClose": {
        // This message is sent whenever window.close() is called within a window
        // that had originally been opened via window.open. Double-check that this is
        // coming from a top-level frame, and then dispatch the DOMWindowClose event
        // on the browser so that the front-end code can do the right thing with the
        // request to close.
        if (!this.manager.browsingContext.parent) {
          let browser = this.manager.browsingContext.embedderElement;
          let win = browser.ownerGlobal;
          // If this is a non-remote browser, the DOMWindowClose event will bubble
          // up naturally, and doesn't need to be re-dispatched.
          if (browser.isRemoteBrowser) {
            browser.dispatchEvent(
              new win.CustomEvent("DOMWindowClose", {
                bubbles: true,
              })
            );
          }
        }
        break;
      }
    }
  }
}
