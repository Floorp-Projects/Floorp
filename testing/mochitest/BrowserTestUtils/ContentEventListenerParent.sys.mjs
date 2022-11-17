/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserTestUtils } from "resource://testing-common/BrowserTestUtils.sys.mjs";

export class ContentEventListenerParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "ContentEventListener:Run": {
        BrowserTestUtils._receivedContentEventListener(
          aMessage.data.listenerId,
          this.browsingContext.browserId
        );
        break;
      }
    }
  }
}
