/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);

const UCT_URI = "chrome://mozapps/content/downloads/unknownContentType.xhtml";
const LOAD_URI =
  "http://mochi.test:8888/browser/toolkit/mozapps/downloads/tests/browser/unknownContentType_dialog_layout_data.txt";

const DIALOG_DELAY =
  Services.prefs.getIntPref("security.dialog_enable_delay") + 200;

let UCTObserver = {
  opened: PromiseUtils.defer(),
  closed: PromiseUtils.defer(),

  observe(aSubject, aTopic, aData) {
    let win = aSubject;

    switch (aTopic) {
      case "domwindowopened":
        win.addEventListener(
          "load",
          function onLoad(event) {
            // Let the dialog initialize
            SimpleTest.executeSoon(function() {
              UCTObserver.opened.resolve(win);
            });
          },
          { once: true }
        );
        break;

      case "domwindowclosed":
        if (win.location == UCT_URI) {
          this.closed.resolve();
        }
        break;
    }
  },
};

function waitDelay(delay) {
  return new Promise((resolve, reject) => {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    window.setTimeout(resolve, delay);
  });
}

add_task(async function test_unknownContentType_delayedbutton() {
  Services.ww.registerNotification(UCTObserver);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: LOAD_URI,
      waitForLoad: false,
      waitForStateStop: true,
    },
    async function() {
      let uctWindow = await UCTObserver.opened.promise;
      let dialog = uctWindow.document.getElementById("unknownContentType");
      let ok = dialog.getButton("accept");

      SimpleTest.is(ok.disabled, true, "button started disabled");

      await waitDelay(DIALOG_DELAY);

      SimpleTest.is(ok.disabled, false, "button was enabled");

      let focusOutOfDialog = SimpleTest.promiseFocus(window);
      window.focus();
      await focusOutOfDialog;

      SimpleTest.is(ok.disabled, true, "button was disabled");

      let focusOnDialog = SimpleTest.promiseFocus(uctWindow);
      uctWindow.focus();
      await focusOnDialog;

      SimpleTest.is(ok.disabled, true, "button remained disabled");

      await waitDelay(DIALOG_DELAY);
      SimpleTest.is(ok.disabled, false, "button re-enabled after delay");

      dialog.cancelDialog();
      await UCTObserver.closed.promise;

      Services.ww.unregisterNotification(UCTObserver);
      uctWindow = null;
      UCTObserver = null;
    }
  );
});
