/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class AppTestDelegateChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded":
      case "load": {
        this.sendAsyncMessage(event.type, {
          internalURL: event.target.documentURI,
          visibleURL: event.target.location?.href,
        });
        break;
      }
    }
  }
}
