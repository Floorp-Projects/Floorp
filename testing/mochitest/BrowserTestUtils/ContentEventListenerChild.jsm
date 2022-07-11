/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContentEventListenerChild"];

class ContentEventListenerChild extends JSWindowActorChild {
  actorCreated() {
    this._contentEvents = new Map();
    this._shutdown = false;
    this._chromeEventHandler = null;
    Services.cpmm.sharedData.addEventListener("change", this);
  }

  didDestroy() {
    this._shutdown = true;
    Services.cpmm.sharedData.removeEventListener("change", this);
    this._updateContentEventListeners(/* clearListeners = */ true);
    if (this._contentEvents.size != 0) {
      throw new Error(`Didn't expect content events after willDestroy`);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "DOMWindowCreated": {
        this._updateContentEventListeners();
        break;
      }

      case "change": {
        if (
          !event.changedKeys.includes("BrowserTestUtils:ContentEventListener")
        ) {
          return;
        }
        this._updateContentEventListeners();
        break;
      }
    }
  }

  /**
   * This method first determines the desired set of content event listeners
   * for the window. This is either the empty set, if clearListeners is true,
   * or is retrieved from the message manager's shared data. It then compares
   * this event listener data to the existing set of listeners that we have
   * registered, as recorded in this._contentEvents. Each content event listener
   * has been assigned a unique id by the parent process. If a listener was
   * added, but is not in the new event data, it is removed. If a listener was
   * not present, but is in the new event data, it is added. If it is in both,
   * then a basic check is done to see if they are the same.
   *
   * @param {bool} clearListeners [optional]
   *        If this is true, then instead of checking shared data to decide
   *        what the desired set of listeners is, just use the empty set. This
   *        will result in any existing listeners being cleared, and is used
   *        when the window is going away.
   */
  _updateContentEventListeners(clearListeners = false) {
    // If we've already begun the destruction process, any new event
    // listeners for our bc id can't possibly really be for us, so ignore them.
    if (this._shutdown && !clearListeners) {
      throw new Error(
        "Tried to update after we shut down content event listening"
      );
    }

    let newEventData;
    if (!clearListeners) {
      newEventData = Services.cpmm.sharedData.get(
        "BrowserTestUtils:ContentEventListener"
      );
    }
    if (!newEventData) {
      newEventData = new Map();
    }

    // Check that entries that continue to exist are the same and remove entries
    // that no longer exist.
    for (let [
      listenerId,
      { eventName, listener, listenerOptions },
    ] of this._contentEvents.entries()) {
      let newData = newEventData.get(listenerId);
      if (newData) {
        if (newData.eventName !== eventName) {
          // Could potentially check if listenerOptions are the same, but
          // checkFnSource can't be checked unless we store it, and this is
          // just a smoke test anyways, so don't bother.
          throw new Error(
            "Got new content event listener that disagreed with existing data"
          );
        }
        continue;
      }
      if (!this._chromeEventHandler) {
        throw new Error(
          "Trying to remove an event listener for waitForContentEvent without a cached event handler"
        );
      }
      this._chromeEventHandler.removeEventListener(
        eventName,
        listener,
        listenerOptions
      );
      this._contentEvents.delete(listenerId);
    }

    let actorChild = this;

    // Add in new entries.
    for (let [
      listenerId,
      { eventName, listenerOptions, checkFnSource },
    ] of newEventData.entries()) {
      let oldData = this._contentEvents.get(listenerId);
      if (oldData) {
        // We checked that the data is the same in the previous loop.
        continue;
      }

      /* eslint-disable no-eval */
      let checkFn;
      if (checkFnSource) {
        checkFn = eval(`(() => (${unescape(checkFnSource)}))()`);
      }
      /* eslint-enable no-eval */

      function listener(event) {
        if (checkFn && !checkFn(event)) {
          return;
        }
        actorChild.sendAsyncMessage("ContentEventListener:Run", {
          listenerId,
        });
      }

      // Cache the chrome event handler because this.docShell won't be
      // available during shut down.
      if (!this._chromeEventHandler) {
        try {
          this._chromeEventHandler = this.docShell.chromeEventHandler;
        } catch (error) {
          if (error.name === "InvalidStateError") {
            // We'll arrive here if we no longer have our manager, so we can
            // just swallow this error.
            continue;
          }
          throw error;
        }
      }

      // Some windows, like top-level browser windows, maybe not have a chrome
      // event handler set up as this point, but we don't actually care about
      // events on those windows, so ignore them.
      if (!this._chromeEventHandler) {
        continue;
      }

      this._chromeEventHandler.addEventListener(
        eventName,
        listener,
        listenerOptions
      );
      this._contentEvents.set(listenerId, {
        eventName,
        listener,
        listenerOptions,
      });
    }

    // If there are no active content events, clear our reference to the chrome
    // event handler to prevent leaks.
    if (this._contentEvents.size == 0) {
      this._chromeEventHandler = null;
    }
  }
}
