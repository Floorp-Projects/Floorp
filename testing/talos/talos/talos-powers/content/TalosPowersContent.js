/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should be executed by the [possibly unprivileged] consumer, e.g.:
// <script src="chrome://talos-powers-content/content/TalosPowersContent.js"></script>
// and then e.g. TalosPowersParent.exec("sampleParentService", myArg, myCallback)
// It marely sends a query event and possibly listens to a reply event, and does not
// depend on any special privileges.

var TalosPowersContent;
var TalosPowersParent;

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
      }
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

  /**
   * Generic interface to service functions which run at the parent process.
   */
  // If including this script proves too much touble, you may embed the following
  // code verbatim instead, and keep the copy up to date with its source here:
  TalosPowersParent = {
    replyId: 1,

    // dispatch an event to the framescript and register the result/callback event
    exec(commandName, arg, callback, opt_custom_window) {
      let win = opt_custom_window || window;
      let replyEvent = "TalosPowers:ParentExec:ReplyEvent:" + this.replyId++;
      if (callback) {
        win.addEventListener(replyEvent, function(e) {
          callback(e.detail);
        }, {once: true});
      }
      win.dispatchEvent(
        new win.CustomEvent("TalosPowers:ParentExec:QueryEvent", {
          bubbles: true,
          detail: {
            command: {
              name: commandName,
              data: arg,
            },
            listeningTo: replyEvent,
          }
        })
      );
    },

  };
  // End of possibly embedded code

})();
