/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentDOMReference } from "resource://gre/modules/ContentDOMReference.sys.mjs";

export class WebChannelChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type === "WebChannelMessageToChrome") {
      return this._onMessageToChrome(event);
    }
    return undefined;
  }

  receiveMessage(msg) {
    if (msg.name === "WebChannelMessageToContent") {
      return this._onMessageToContent(msg);
    }
    return undefined;
  }

  _onMessageToChrome(e) {
    if (e.detail) {
      if (typeof e.detail != "string") {
        console.error("WebChannelMessageToChrome must only send strings");
        return;
      }

      let eventTarget =
        e.target instanceof Ci.nsIDOMWindow
          ? null
          : ContentDOMReference.get(e.target);
      this.sendAsyncMessage("WebChannelMessageToChrome", {
        contentData: e.detail,
        eventTarget,
      });
    } else {
      console.error("WebChannel message failed. No message detail.");
    }
  }

  _onMessageToContent(msg) {
    if (msg.data && this.contentWindow) {
      // msg.objects.eventTarget will be defined if sending a response to
      // a WebChannelMessageToChrome event. An unsolicited send
      // may not have an eventTarget defined, in this case send to the
      // main content window.
      let { eventTarget, principal } = msg.data;
      if (!eventTarget) {
        eventTarget = this.contentWindow;
      } else {
        eventTarget = ContentDOMReference.resolve(eventTarget);
      }
      if (!eventTarget) {
        console.error("WebChannel message failed. No target.");
        return;
      }

      // Use nodePrincipal if available, otherwise fallback to document principal.
      let targetPrincipal =
        eventTarget instanceof Ci.nsIDOMWindow
          ? eventTarget.document.nodePrincipal
          : eventTarget.nodePrincipal;

      if (principal.subsumes(targetPrincipal)) {
        let targetWindow = this.contentWindow;
        eventTarget.dispatchEvent(
          new targetWindow.CustomEvent("WebChannelMessageToContent", {
            detail: Cu.cloneInto(
              {
                id: msg.data.id,
                message: msg.data.message,
              },
              targetWindow
            ),
          })
        );
      } else {
        console.error("WebChannel message failed. Principal mismatch.");
      }
    } else {
      console.error("WebChannel message failed. No message data.");
    }
  }
}
