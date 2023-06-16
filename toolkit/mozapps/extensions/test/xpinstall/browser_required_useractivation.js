/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const XPI_URL = `${TESTROOT}amosigned.xpi`;

async function runTestCase(spawnArgs, spawnFn, { expectInstall, clickLink }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Make use the user activation requirements is enabled while running this test.
      ["xpinstall.userActivation.required", true],
    ],
  });
  await BrowserTestUtils.withNewTab(TESTROOT, async browser => {
    const expectedError = `${XPI_URL} install cancelled because of missing user gesture activation`;

    let promiseDone;

    if (expectInstall) {
      promiseDone = TestUtils.topicObserved("addon-install-blocked").then(
        ([subject]) => {
          // Cancel the pending installation flow.
          subject.wrappedJSObject.cancel();
        }
      );
    } else {
      promiseDone = new Promise(resolve => {
        function messageHandler(msgObj) {
          if (
            msgObj instanceof Ci.nsIScriptError &&
            msgObj.message.includes(expectedError)
          ) {
            ok(
              true,
              "Expect error on triggering navigation to xpi without user gesture activation"
            );
            cleanupListener();
            resolve();
          }
        }
        let listenerCleared = false;
        function cleanupListener() {
          if (!listenerCleared) {
            Services.console.unregisterListener(messageHandler);
          }
          listenerCleared = true;
        }
        Services.console.registerListener(messageHandler);
        registerCleanupFunction(cleanupListener);
      });
    }

    await SpecialPowers.spawn(browser, spawnArgs, spawnFn);

    if (clickLink) {
      info("Click link element");
      // Wait for the install to trigger the third party website doorhanger.
      // Trigger the link by simulating a mouse click, and expect it to trigger the
      // install flow instead (the window is still navigated to the xpi url from the
      // webpage JS code, but doing it while handling a DOM event does make it pass
      // the user activation check).
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#link-to-xpi-file",
        {},
        browser
      );
    }

    info("Wait test case to be completed");
    await promiseDone;
    ok(true, "Test case run completed");
  });
}

add_task(async function testSuccessOnUserActivatedLink() {
  await runTestCase(
    [XPI_URL],
    xpiURL => {
      const { document } = this.content;
      const link = document.createElement("a");
      link.id = "link-to-xpi-file";
      link.setAttribute("href", xpiURL);
      link.textContent = "Link to XPI File";

      // Empty the test case and add the link, if the link is not visible
      // without scrolling, BrowserTestUtils.synthesizeMouseAtCenter may
      // fail to trigger the mouse event.
      document.body.innerHTML = "";
      document.body.appendChild(link);
    },
    { expectInstall: true, clickLink: true }
  );
});

add_task(async function testSuccessOnJSWithUserActivation() {
  await runTestCase(
    [XPI_URL],
    xpiURL => {
      const { document } = this.content;
      const link = document.createElement("a");
      link.id = "link-to-xpi-file";
      link.setAttribute("href", "#");
      link.textContent = "Link to XPI File";

      // Empty the test case and add the link, if the link is not visible
      // without scrolling, BrowserTestUtils.synthesizeMouseAtCenter may
      // fail to trigger the mouse event.
      document.body.innerHTML = "";
      document.body.appendChild(link);

      this.content.eval(`
        const linkEl = document.querySelector("#link-to-xpi-file");
        linkEl.onclick = () => {
          // This is expected to trigger the install flow successfully if handling
          // a user gesture DOM event, but to fail when triggered outside of it (as
          // done a few line below).
          window.location = "${xpiURL}";
        };
      `);
    },
    { expectInstall: true, clickLink: true }
  );
});

add_task(async function testFailureOnJSWithoutUserActivation() {
  await runTestCase(
    [XPI_URL],
    xpiURL => {
      this.content.eval(`window.location = "${xpiURL}";`);
    },
    { expectInstall: false }
  );
});

add_task(async function testFailureOnJSWithoutUserActivation() {
  await runTestCase(
    [XPI_URL],
    xpiURL => {
      this.content.eval(`
        const frame = document.createElement("iframe");
        frame.src = "${xpiURL}";
        document.body.innerHTML = "";
        document.body.appendChild(frame);
      `);
    },
    { expectInstall: false }
  );
});
