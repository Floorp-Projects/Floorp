/* eslint-env webextensions */
"use strict";

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

let PREF_NAME = "browser.tabs.remote.useHTTPResponseProcessSelection";

async function performLoad(browser, opts, action) {
  let loadedPromise = BrowserTestUtils.browserLoaded(
    browser,
    false,
    opts.url,
    opts.maybeErrorPage
  );
  await action();
  await loadedPromise;
}

const EXTENSION_DATA = {
  manifest: {
    name: "Simple extension test",
    version: "1.0",
    manifest_version: 2,
    description: "",

    permissions: ["proxy", "webRequest", "webRequestBlocking", "<all_urls>"],
  },

  files: {
    "dummy.html": "<html>webext dummy</html>",
  },

  async background() {
    browser.test.log("background script running");
    browser.webRequest.onAuthRequired.addListener(
      async details => {
        browser.test.log("webRequest onAuthRequired");

        // A blocking request that returns a promise exercises a codepath that
        // sets the notificationCallbacks on the channel to a JS object that we
        // can't do directly QueryObject on with expected results.
        // This triggered a crash which was fixed in bug 1528188.
        return new Promise(async (resolve, reject) => {
          setTimeout(resolve, 0);
        });
      },
      { urls: ["*://*/*"] },
      ["blocking"]
    );
  },
};

async function withExtensionDummy(callback) {
  let extension = ExtensionTestUtils.loadExtension(EXTENSION_DATA);
  await extension.startup();
  let rv = await callback(`moz-extension://${extension.uuid}/dummy.html`);
  await extension.unload();
  return rv;
}

const PRINT_POSTDATA = httpURL("print_postdata.sjs");
const FILE_DUMMY = fileURL("dummy_page.html");

async function postFrom(start, target) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: start,
    },
    async function(browser) {
      info("Test tab ready: " + start);

      // Create the form element in our loaded URI.
      await ContentTask.spawn(browser, { target }, function({ target }) {
        // eslint-disable-next-line no-unsanitized/property
        content.document.body.innerHTML = `
        <form method="post" action="${target}">
          <input type="text" name="initialRemoteType" value="${
            Services.appinfo.remoteType
          }">
          <input type="submit" id="submit">
        </form>`;
      });

      // Perform a form POST submit load.
      info("Performing POST submission");
      await performLoad(
        browser,
        {
          url(url) {
            return url == PRINT_POSTDATA || url == target;
          },
          maybeErrorPage: true,
        },
        async () => {
          await ContentTask.spawn(browser, null, () => {
            content.document.querySelector("#submit").click();
          });
        }
      );

      // Check that the POST data was submitted.
      info("Fetching results");
      return ContentTask.spawn(browser, null, () => {
        return {
          remoteType: Services.appinfo.remoteType,
          location: "" + content.location.href,
          body: content.document.body.textContent,
        };
      });
    }
  );
}

add_task(async function test_disabled() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_NAME, false]] });

  // With the pref disabled, file URIs should successfully POST, but remain in
  // the 'file' process.
  info("DISABLED -- FILE -- raw URI load");
  let resp = await postFrom(FILE_DUMMY, PRINT_POSTDATA);
  is(resp.remoteType, E10SUtils.FILE_REMOTE_TYPE, "no process switch");
  is(resp.location, PRINT_POSTDATA, "correct location");
  is(resp.body, "initialRemoteType=file", "correct POST body");

  info("DISABLED -- FILE -- 307-redirect URI load");
  let resp307 = await postFrom(FILE_DUMMY, add307(PRINT_POSTDATA));
  is(resp307.remoteType, E10SUtils.FILE_REMOTE_TYPE, "no process switch");
  is(resp307.location, PRINT_POSTDATA, "correct location");
  is(resp307.body, "initialRemoteType=file", "correct POST body");

  // With the pref disabled, extension URIs should fail to POST, but correctly
  // switch processes.
  await withExtensionDummy(async dummy => {
    info("DISABLED -- EXTENSION -- raw URI load");
    let respExt = await postFrom(dummy, PRINT_POSTDATA);
    is(respExt.remoteType, E10SUtils.WEB_REMOTE_TYPE, "process switch");
    is(respExt.location, PRINT_POSTDATA, "correct location");
    is(respExt.body, "", "no POST body");

    info("DISABLED -- EXTENSION -- 307-redirect URI load");
    let respExt307 = await postFrom(dummy, add307(PRINT_POSTDATA));
    is(respExt307.remoteType, E10SUtils.WEB_REMOTE_TYPE, "process switch");
    is(respExt307.location, PRINT_POSTDATA, "correct location");
    is(respExt307.body, "", "no POST body");
  });
});

add_task(async function test_enabled() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_NAME, true]] });

  // With the pref enabled, URIs should correctly switch processes & the POST
  // should succeed.
  info("ENABLED -- FILE -- raw URI load");
  let resp = await postFrom(FILE_DUMMY, PRINT_POSTDATA);
  is(resp.remoteType, E10SUtils.WEB_REMOTE_TYPE, "process switch");
  is(resp.location, PRINT_POSTDATA, "correct location");
  is(resp.body, "initialRemoteType=file", "correct POST body");

  info("ENABLED -- FILE -- 307-redirect URI load");
  let resp307 = await postFrom(FILE_DUMMY, add307(PRINT_POSTDATA));
  is(resp307.remoteType, E10SUtils.WEB_REMOTE_TYPE, "process switch");
  is(resp307.location, PRINT_POSTDATA, "correct location");
  is(resp307.body, "initialRemoteType=file", "correct POST body");

  // Same with extensions
  await withExtensionDummy(async dummy => {
    info("ENABLED -- EXTENSION -- raw URI load");
    let respExt = await postFrom(dummy, PRINT_POSTDATA);
    is(respExt.remoteType, E10SUtils.WEB_REMOTE_TYPE, "process switch");
    is(respExt.location, PRINT_POSTDATA, "correct location");
    is(respExt.body, "initialRemoteType=extension", "correct POST body");

    info("ENABLED -- EXTENSION -- 307-redirect URI load");
    let respExt307 = await postFrom(dummy, add307(PRINT_POSTDATA));
    is(respExt307.remoteType, E10SUtils.WEB_REMOTE_TYPE, "process switch");
    is(respExt307.location, PRINT_POSTDATA, "correct location");
    is(respExt307.body, "initialRemoteType=extension", "correct POST body");
  });
});
