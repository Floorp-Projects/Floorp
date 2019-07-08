/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Observer"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class Observer {
  static observe(type, observer) {
    Services.obs.addObserver(observer, type);
  }

  static unobserve(type, observer) {
    Services.obs.removeObserver(observer, type);
  }

  static once(type, observer = () => {}) {
    return new Promise(resolve => {
      const wrappedObserver = (first, ...rest) => {
        Observer.unobserve(type, wrappedObserver);
        observer.call(first, ...rest);
        resolve();
      };
      Observer.observe(type, wrappedObserver);
    });
  }
}
