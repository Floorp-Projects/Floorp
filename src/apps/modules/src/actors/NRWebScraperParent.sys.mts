/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWebScraperParent extends JSWindowActorParent {
  receiveMessage(
    message: {
      name: string;
      data?: { selector: string; value: string };
    },
  ) {
    // Forward all messages to the child and return the result.
    return this.sendQuery(message.name, message.data);
  }
}
