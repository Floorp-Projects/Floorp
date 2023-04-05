/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class Domain {
  constructor(session) {
    this.session = session;
    this.name = this.constructor.name;

    this.eventListeners_ = new Set();
    this._requestCounter = 0;
  }

  destructor() {}

  emit(eventName, params = {}) {
    for (const listener of this.eventListeners_) {
      try {
        if (isEventHandler(listener)) {
          listener.onEvent(eventName, params);
        } else {
          listener.call(this, eventName, params);
        }
      } catch (e) {
        console.error(e);
      }
    }
  }

  /**
   * Execute the provided method in the child domain that has the same domain
   * name. eg. calling this.executeInChild from domains/parent/Input.jsm will
   * attempt to execute the method in domains/content/Input.jsm.
   *
   * This can only be called from parent domains managed by a TabSession.
   *
   * @param {string} method
   *        Name of the method to call on the child domain.
   * @param {object} params
   *        Optional parameters. Must be serializable.
   */
  executeInChild(method, params) {
    if (!this.session.executeInChild) {
      throw new Error(
        "executeInChild can only be used in Domains managed by a TabSession"
      );
    }
    this._requestCounter++;
    const id = this.name + "-" + this._requestCounter;
    return this.session.executeInChild(id, this.name, method, params);
  }

  addEventListener(listener) {
    if (typeof listener != "function" && !isEventHandler(listener)) {
      throw new TypeError();
    }
    this.eventListeners_.add(listener);
  }

  // static

  static implements(command) {
    return command && typeof this.prototype[command] == "function";
  }
}

function isEventHandler(listener) {
  return (
    listener && "onEvent" in listener && typeof listener.onEvent == "function"
  );
}
