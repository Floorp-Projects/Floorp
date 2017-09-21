/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

this.EXPORTED_SYMBOLS = [
  "ContentEventObserverService",
  "WebElementEventTarget",
];

/**
 * The {@link EventTarget} for web elements can be used to observe DOM
 * events in the content document.
 *
 * A caveat of the current implementation is that it is only possible
 * to listen for top-level <code>window</code> global events.
 *
 * It needs to be backed by a {@link ContentEventObserverService} in a
 * content frame script.
 *
 * Usage:
 *
 * <pre><code>
 *     let observer = new WebElementEventTarget(messageManager);
 *     await new Promise(resolve => {
 *       observer.addEventListener("visibilitychange", resolve, {once: true});
 *       chromeWindow.minimize();
 *     });
 * </code></pre>
 */
class WebElementEventTarget {
  /**
   * @param {function(): nsIMessageListenerManager} messageManagerFn
   *     Message manager to the current browser.
   */
  constructor(messageManager) {
    this.mm = messageManager;
    this.listeners = {};
    this.mm.addMessageListener("Marionette:DOM:OnEvent", this);
  }

  /**
   * Register an event handler of a specific event type from the content
   * frame.
   *
   * @param {string} type
   *     Event type to listen for.
   * @param {EventListener} listener
   *     Object which receives a notification (a {@link BareEvent})
   *     when an event of the specified type occurs.  This must be
   *     an object implementing the {@link EventListener} interface,
   *     or a JavaScript function.
   * @param {boolean=} once
   *     Indicates that the <var>listener</var> should be invoked at
   *     most once after being added.  If true, the <var>listener</var>
   *     would automatically be removed when invoked.
   */
  addEventListener(type, listener, {once = false} = {}) {
    if (!(type in this.listeners)) {
      this.listeners[type] = [];
    }

    if (!this.listeners[type].includes(listener)) {
      listener.once = once;
      this.listeners[type].push(listener);
    }

    this.mm.sendAsyncMessage("Marionette:DOM:AddEventListener", {type});
  }

  /**
   * Removes an event listener.
   *
   * @param {string} type
   *     Type of event to cease listening for.
   * @param {EventListener} listener
   *     Event handler to remove from the event target.
   */
  removeEventListener(type, listener) {
    if (!(type in this.listeners)) {
      return;
    }

    let stack = this.listeners[type];
    for (let i = stack.length - 1; i >= 0; --i) {
      if (stack[i] === listener) {
        stack.splice(i, 1);
        if (stack.length == 0) {
          this.mm.sendAsyncMessage("Marionette:DOM:RemoveEventListener", {type});
        }
        return;
      }
    }
  }

  dispatchEvent(event) {
    if (!(event.type in this.listeners)) {
      return;
    }

    event.target = this;

    let stack = this.listeners[event.type].slice(0);
    stack.forEach(listener => {
      listener.call(this, event);

      if (listener.once) {
        this.removeEventListener(event.type, listener);
      }
    });
  }

  receiveMessage({target, name, data, objects}) {
    if (name != "Marionette:DOM:OnEvent") {
      return;
    }

    let ev = {
      type: data.type,
      target: objects.target,
    };
    this.dispatchEvent(ev);
  }
}
this.WebElementEventTarget = WebElementEventTarget;

/**
 * Provides the frame script backend for the
 * {@link WebElementEventTarget}.
 *
 * This service receives requests for new DOM events to listen for and
 * to cease listening for, and despatches IPC messages to the browser
 * when they fire.
 */
class ContentEventObserverService {
  /**
   * @param {WindowProxy} windowGlobal
   *     Window.
   * @param {nsIMessageSender.sendAsyncMessage} sendAsyncMessage
   *     Function for sending an async message to the parent browser.
   */
  constructor(windowGlobal, sendAsyncMessage) {
    this.window = windowGlobal;
    this.sendAsyncMessage = sendAsyncMessage;
    this.events = new Set();
  }

  /**
   * Observe a new DOM event.
   *
   * When the DOM event of <var>type</var> fires, a message is passed
   * to the parent browser's event observer.
   *
   * If event type is already being observed, only a single message
   * is sent.  E.g. multiple registration for events will only ever emit
   * a maximum of one message.
   *
   * @param {string} type
   *     DOM event to listen for.
   */
  add(type) {
    if (this.events.has(type)) {
      return;
    }
    this.window.addEventListener(type, this);
    this.events.add(type);
  }

  /**
   * Ceases observing a DOM event.
   *
   * @param {string} type
   *     DOM event to stop listening for.
   */
  remove(type) {
    if (!this.events.has(type)) {
      return;
    }
    this.window.removeEventListener(type, this);
    this.events.delete(type);
  }

  /** Ceases observing all previously registered DOM events. */
  clear() {
    for (let ev of this) {
      this.remove(ev);
    }
  }

  * [Symbol.iterator]() {
    for (let ev of this.events) {
      yield ev;
    }
  }

  handleEvent({type, target}) {
    this.sendAsyncMessage("Marionette:DOM:OnEvent", {type}, {target});
  }
}
this.ContentEventObserverService = ContentEventObserverService;
