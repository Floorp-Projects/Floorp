/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MessageHandlerInfo"];

class MessageHandlerInfo {
  /**
   * The MessageHandlerInfo class contains all the metadata identifying a
   * MessageHandler instance.
   *
   * @param {String} sessionId
   *     MID of the session the handler is used for.
   * @param {String} type
   *     MessageHandler type, one of MessageHandler.type.
   * @param {String} contextId
   *     Unique id for the context of a MessageHandler.
   */
  constructor(sessionId, type, contextId) {
    this.sessionId = sessionId;
    this.type = type;
    this.contextId = contextId;

    // Create a composite key based on the sessionId, MessageHandler type and
    // context id. This key will uniquely match at most one MessageHandler
    // instance.
    this.key = [this.sessionId, this.type, this.contextId].join("-");
  }
}
