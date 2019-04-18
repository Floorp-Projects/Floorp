/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["MessageManagerProxy"];

const {ExtensionUtils} = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const {
  DefaultMap,
} = ExtensionUtils;

/**
 * Acts as a proxy for a message manager or message manager owner, and
 * tracks docShell swaps so that messages are always sent to the same
 * receiver, even if it is moved to a different <browser>.
 *
 * @param {nsIMessageSender|Element} target
 *        The target message manager on which to send messages, or the
 *        <browser> element which owns it.
 */
class MessageManagerProxy {
  constructor(target) {
    this.listeners = new DefaultMap(() => new Map());
    this.closed = false;

    if (target instanceof Ci.nsIMessageSender) {
      this.messageManager = target;
    } else {
      this.addListeners(target);
    }

    Services.obs.addObserver(this, "message-manager-close");
  }

  /**
   * Disposes of the proxy object, removes event listeners, and drops
   * all references to the underlying message manager.
   *
   * Must be called before the last reference to the proxy is dropped,
   * unless the underlying message manager or <browser> is also being
   * destroyed.
   */
  dispose() {
    if (this.eventTarget) {
      this.removeListeners(this.eventTarget);
      this.eventTarget = null;
    }
    this.messageManager = null;

    Services.obs.removeObserver(this, "message-manager-close");
  }

  observe(subject, topic, data) {
    if (topic === "message-manager-close") {
      if (subject === this.messageManager) {
        this.closed = true;
      }
    }
  }

  /**
   * Returns true if the given target is the same as, or owns, the given
   * message manager.
   *
   * @param {nsIMessageSender|MessageManagerProxy|Element} target
   *        The message manager, MessageManagerProxy, or <browser>
   *        element against which to match.
   * @param {nsIMessageSender} messageManager
   *        The message manager against which to match `target`.
   *
   * @returns {boolean}
   *        True if `messageManager` is the same object as `target`, or
   *        `target` is a MessageManagerProxy or <browser> element that
   *        is tied to it.
   */
  static matches(target, messageManager) {
    return target === messageManager || target.messageManager === messageManager;
  }

  /**
   * @property {nsIMessageSender|null} messageManager
   *        The message manager that is currently being proxied. This
   *        may change during the life of the proxy object, so should
   *        not be stored elsewhere.
   */

  /**
   * Sends a message on the proxied message manager.
   *
   * @param {array} args
   *        Arguments to be passed verbatim to the underlying
   *        sendAsyncMessage method.
   * @returns {undefined}
   */
  sendAsyncMessage(...args) {
    if (this.messageManager) {
      return this.messageManager.sendAsyncMessage(...args);
    }

    Cu.reportError(`Cannot send message: Other side disconnected: ${uneval(args)}`);
  }

  get isDisconnected() {
    return this.closed || !this.messageManager;
  }

  /**
   * Adds a message listener to the current message manager, and
   * transfers it to the new message manager after a docShell swap.
   *
   * @param {string} message
   *        The name of the message to listen for.
   * @param {nsIMessageListener} listener
   *        The listener to add.
   * @param {boolean} [listenWhenClosed = false]
   *        If true, the listener will receive messages which were sent
   *        after the remote side of the listener began closing.
   */
  addMessageListener(message, listener, listenWhenClosed = false) {
    this.messageManager.addMessageListener(message, listener, listenWhenClosed);
    this.listeners.get(message).set(listener, listenWhenClosed);
  }

  /**
   * Adds a message listener from the current message manager.
   *
   * @param {string} message
   *        The name of the message to stop listening for.
   * @param {nsIMessageListener} listener
   *        The listener to remove.
   */
  removeMessageListener(message, listener) {
    this.messageManager.removeMessageListener(message, listener);

    let listeners = this.listeners.get(message);
    listeners.delete(listener);
    if (!listeners.size) {
      this.listeners.delete(message);
    }
  }

  /**
   * @private
   * Iterates over all of the currently registered message listeners.
   */
  * iterListeners() {
    for (let [message, listeners] of this.listeners) {
      for (let [listener, listenWhenClosed] of listeners) {
        yield {message, listener, listenWhenClosed};
      }
    }
  }

  /**
   * @private
   * Adds docShell swap listeners to the message manager owner.
   *
   * @param {Element} target
   *        The target element.
   */
  addListeners(target) {
    target.addEventListener("SwapDocShells", this);

    this.eventTarget = target;
    this.messageManager = target.messageManager;

    for (let {message, listener, listenWhenClosed} of this.iterListeners()) {
      this.messageManager.addMessageListener(message, listener, listenWhenClosed);
    }
  }

  /**
   * @private
   * Removes docShell swap listeners to the message manager owner.
   *
   * @param {Element} target
   *        The target element.
   */
  removeListeners(target) {
    target.removeEventListener("SwapDocShells", this);

    for (let {message, listener} of this.iterListeners()) {
      this.messageManager.removeMessageListener(message, listener);
    }
  }

  handleEvent(event) {
    if (event.type == "SwapDocShells") {
      this.removeListeners(this.eventTarget);
      // The SwapDocShells event is dispatched for both browsers that are being
      // swapped. To avoid double-swapping, register the event handler after
      // both SwapDocShells events have fired.
      this.eventTarget.addEventListener("EndSwapDocShells", () => {
        this.addListeners(event.detail);
      }, {once: true});
    }
  }
}
