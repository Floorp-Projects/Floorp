/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The unknownContentType popup can have two different layouts depending on
 * whether a helper application can be selected or not.
 * This tests that both layouts have correct collapsed elements.
 */

const UCT_URI = "chrome://mozapps/content/downloads/unknownContentType.xhtml";

let tests = [
  {
    // This URL will trigger the simple UI, where only the Save an Cancel buttons are available
    url:
      "http://mochi.test:8888/browser/toolkit/mozapps/downloads/tests/browser/unknownContentType_dialog_layout_data.pif",
    elements: {
      basicBox: { collapsed: false },
      normalBox: { collapsed: true },
    },
  },
  {
    // This URL will trigger the full UI
    url:
      "http://mochi.test:8888/browser/toolkit/mozapps/downloads/tests/browser/unknownContentType_dialog_layout_data.txt",
    elements: {
      basicBox: { collapsed: true },
      normalBox: { collapsed: false },
    },
  },
];

function waitDelay(delay) {
  return new Promise((resolve, reject) => {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    window.setTimeout(resolve, delay);
  });
}

add_task(async function test_unknownContentType_dialog_layout() {
  for (let test of tests) {
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

    Services.ww.registerNotification(UCTObserver);
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: test.url,
        waitForLoad: false,
        waitForStateStop: true,
      },
      async function() {
        // If browser.download.improvements_to_download_panel pref is enabled,
        // the unknownContentType will not appear by default.
        // So wait an amount of time to ensure it hasn't opened.
        let windowOpenDelay = waitDelay(1000);
        let uctWindow = await Promise.race([
          windowOpenDelay,
          UCTObserver.opened.promise,
        ]);
        const prefEnabled = Services.prefs.getBoolPref(
          "browser.download.improvements_to_download_panel"
        );

        if (prefEnabled) {
          SimpleTest.is(
            !uctWindow,
            true,
            "UnknownContentType window shouldn't open."
          );
          return;
        }

        for (let [id, props] of Object.entries(test.elements)) {
          let elem = uctWindow.dialog.dialogElement(id);
          for (let [prop, value] of Object.entries(props)) {
            SimpleTest.is(
              elem[prop],
              value,
              "Element with id " +
                id +
                " has property " +
                prop +
                " set to " +
                value
            );
          }
        }
        let focusOnDialog = SimpleTest.promiseFocus(uctWindow);
        uctWindow.focus();
        await focusOnDialog;

        uctWindow.document.getElementById("unknownContentType").cancelDialog();
        uctWindow = null;
        Services.ww.unregisterNotification(UCTObserver);
      }
    );
  }
});
