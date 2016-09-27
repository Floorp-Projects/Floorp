/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TalosPowersContent;

(function() {
  TalosPowersContent = {
    /**
     * Synchronously force CC and GC in this process, as well as in the
     * parent process.
     */
    forceCCAndGC() {
      var event = new CustomEvent("TalosPowersContentForceCCAndGC", {
        bubbles: true,
      });
      document.dispatchEvent(event);
    },

    focus(callback) {
      if (callback) {
        addEventListener("TalosPowersContentFocused", function focused() {
          removeEventListener("TalosPowersContentFocused", focused);
          callback();
        });
      };
      document.dispatchEvent(new CustomEvent("TalosPowersContentFocus", {
        bubbles: true,
      }));
    },

    getStartupInfo() {
      return new Promise((resolve) => {
        var event = new CustomEvent("TalosPowersContentGetStartupInfo", {
          bubbles: true,
        });
        document.dispatchEvent(event);

        addEventListener("TalosPowersContentGetStartupInfoResult",
                         function onResult(e) {
          removeEventListener("TalosPowersContentGetStartupInfoResult",
                              onResult);
          resolve(e.detail);
        });
      });
    },
  };
})();
