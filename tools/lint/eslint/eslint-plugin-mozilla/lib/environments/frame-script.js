/**
 * @fileoverview Defines the environment for frame scripts.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  globals: {
    // dom/chrome-webidl/MessageManager.webidl

    // MessageManagerGlobal
    dump: false,
    atob: false,
    btoa: false,

    // MessageListenerManagerMixin
    addMessageListener: false,
    removeMessageListener: false,
    addWeakMessageListener: false,
    removeWeakMessageListener: false,

    // MessageSenderMixin
    sendAsyncMessage: false,
    processMessageManager: false,
    remoteType: false,

    // SyncMessageSenderMixin
    sendSyncMessage: false,

    // ContentFrameMessageManager
    content: false,
    docShell: false,
    tabEventTarget: false,
  },
};
