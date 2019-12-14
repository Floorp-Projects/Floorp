/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests that the basic auth dialog can not be used for DOS attacks
// and that the protections are reset on user-initiated navigation/reload.

const PROMPT_URL = "chrome://global/content/commonDialog.xhtml";
function promiseAuthWindowShown() {
  return new Promise(resolve => {
    let listener = {
      onOpenWindow(xulWin) {
        let domwindow = xulWin.docShell.domWindow;
        waitForFocus(() => {
          is(
            domwindow.document.location.href,
            PROMPT_URL,
            "Should have seen a prompt window"
          );
          is(
            domwindow.args.promptType,
            "promptUserAndPass",
            "Should be an authenticate prompt"
          );
          domwindow.document.getElementById("commonDialog").cancelDialog();
          Services.wm.removeListener(listener);
          resolve();
        }, domwindow);
      },

      onCloseWindow() {},
    };

    Services.wm.addListener(listener);
  });
}

add_task(async function test() {
  await BrowserTestUtils.withNewTab("https://example.com", async function(
    browser
  ) {
    let cancelDialogLimit = Services.prefs.getIntPref(
      "prompts.authentication_dialog_abuse_limit"
    );

    let authShown = promiseAuthWindowShown();
    let browserLoaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.loadURI(
      browser,
      "https://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs"
    );
    await authShown;
    ok(true, "Seen dialog number 1");
    await browserLoaded;
    ok(true, "Loaded document number 1");

    // Reload the document a bit more often than should be allowed.
    // As long as we're in the acceptable range we should receive
    // auth prompts, otherwise we should not receive them and the
    // page should just load.
    // We've already seen the dialog once, hence we start the loop at 1.
    for (let i = 1; i < cancelDialogLimit + 2; i++) {
      if (i < cancelDialogLimit) {
        authShown = promiseAuthWindowShown();
      }
      browserLoaded = BrowserTestUtils.browserLoaded(browser);
      SpecialPowers.spawn(browser, [], function() {
        content.document.location.reload();
      });
      if (i < cancelDialogLimit) {
        await authShown;
        ok(true, `Seen dialog number ${i + 1}`);
      }
      await browserLoaded;
      ok(true, `Loaded document number ${i + 1}`);
    }

    let reloadButton = document.getElementById("reload-button");
    await TestUtils.waitForCondition(
      () => !reloadButton.hasAttribute("disabled")
    );

    // Verify that we can click the reload button to reset the counter.
    authShown = promiseAuthWindowShown();
    browserLoaded = BrowserTestUtils.browserLoaded(browser);
    reloadButton.click();
    await authShown;
    ok(true, "Seen dialog number 1");
    await browserLoaded;
    ok(true, "Loaded document number 1");

    // Now check loading subresources with auth on the page.
    browserLoaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.loadURI(browser, "https://example.com");
    await browserLoaded;

    // We've already seen the dialog once, hence we start the loop at 1.
    for (let i = 1; i < cancelDialogLimit + 2; i++) {
      if (i < cancelDialogLimit) {
        authShown = promiseAuthWindowShown();
      }

      let iframeLoaded = SpecialPowers.spawn(browser, [], async function() {
        let doc = content.document;
        let iframe = doc.createElement("iframe");
        doc.body.appendChild(iframe);
        let loaded = new Promise(resolve => {
          iframe.addEventListener(
            "load",
            function(e) {
              resolve();
            },
            { once: true }
          );
        });
        iframe.src =
          "https://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs";
        await loaded;
      });

      if (i < cancelDialogLimit) {
        await authShown;
        ok(true, `Seen dialog number ${i + 1}`);
      }

      await iframeLoaded;
      ok(true, `Loaded iframe number ${i + 1}`);
    }

    // Verify that third party subresources can not spawn new auth dialogs.
    let iframeLoaded = SpecialPowers.spawn(browser, [], async function() {
      let doc = content.document;
      let iframe = doc.createElement("iframe");
      doc.body.appendChild(iframe);
      let loaded = new Promise(resolve => {
        iframe.addEventListener(
          "load",
          function(e) {
            resolve();
          },
          { once: true }
        );
      });
      iframe.src =
        "https://example.org/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs";
      await loaded;
    });

    await iframeLoaded;
    ok(true, "Loaded a third party iframe without showing the auth dialog");

    // Verify that pressing enter in the urlbar also resets the counter.
    authShown = promiseAuthWindowShown();
    browserLoaded = BrowserTestUtils.browserLoaded(browser);
    gURLBar.value =
      "https://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs";
    gURLBar.focus();
    EventUtils.synthesizeKey("KEY_Enter");
    await authShown;
    await browserLoaded;
  });
});
