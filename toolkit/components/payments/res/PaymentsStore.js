/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The PaymentsStore class provides lightweight storage with an async publish/subscribe mechanism.
 * Synchronous state changes are batched to improve application performance and to reduce partial
 * state propagation.
 */

/* exported PaymentsStore */

class PaymentsStore {
  /**
   * @param {object} [defaultState = {}] The initial state of the store.
   */
  constructor(defaultState = {}) {
    this._state = defaultState;
    this._nextNotifification = 0;
    this._subscribers = new Set();
  }

  /**
   * Get the current state as a shallow clone with a shallow freeze.
   * You shouldn't modify any part of the returned state object as that would bypass notifying
   * subscribers and could lead to subscribers assuming old state.
   *
   * @returns {Object} containing the current state
   */
  getState() {
    return Object.freeze(Object.assign({}, this._state));
  }

  /**
   * Augment the current state with the keys of `obj` and asynchronously notify
   * state subscribers. As a result, multiple synchronous state changes will lead
   * to a single subscriber notification which leads to better performance and
   * reduces partial state changes.
   *
   * @param {Object} obj The object to augment the state with. Keys in the object
   *                     will be shallow copied with Object.assign.
   *
   * @example If the state is currently {a:3} then setState({b:"abc"}) will result in a state of
   *          {a:3, b:"abc"}.
   */
  async setState(obj) {
    Object.assign(this._state, obj);
    let thisChangeNum = ++this._nextNotifification;

    // Let any synchronous setState calls that happen after the current setState call
    // complete first.
    // Their effects on the state will be batched up before the callback is actually called below.
    await Promise.resolve();

    // Don't notify for state changes that are no longer the most recent. We only want to call the
    // callback once with the latest state.
    if (thisChangeNum !== this._nextNotifification) {
      return;
    }

    for (let subscriber of this._subscribers) {
      try {
        subscriber.stateChangeCallback(this.getState());
      } catch (ex) {
        console.error(ex);
      }
    }
  }

  /**
   * Subscribe the object to state changes notifications via a `stateChangeCallback` method.
   *
   * @param {Object} component to receive state change callbacks via a `stateChangeCallback` method.
   *                           If the component is already subscribed, do nothing.
   */
  subscribe(component) {
    if (this._subscribers.has(component)) {
      return;
    }

    this._subscribers.add(component);
  }

  /**
   * @param {Object} component to stop receiving state change callbacks.
   */
  unsubscribe(component) {
    this._subscribers.delete(component);
  }
}
