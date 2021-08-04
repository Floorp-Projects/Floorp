/* eslint-env webextensions */
"use strict";

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

const PRINT_POSTDATA = httpURL("print_postdata.sjs");
const FILE_DUMMY = fileURL("dummy_page.html");
const DATA_URL = "data:text/html,Hello%2C World!";
const DATA_STRING = "Hello, World!";

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
    "redirect.html": "<html>webext redirect</html>",
  },

  extUrl: "",

  async background() {
    browser.test.log("background script running");
    browser.webRequest.onAuthRequired.addListener(
      async details => {
        browser.test.log("webRequest onAuthRequired");

        // A blocking request that returns a promise exercises a codepath that
        // sets the notificationCallbacks on the channel to a JS object that we
        // can't do directly QueryObject on with expected results.
        // This triggered a crash which was fixed in bug 1528188.
        return new Promise((resolve, reject) => {
          setTimeout(resolve, 0);
        });
      },
      { urls: ["*://*/*"] },
      ["blocking"]
    );
    browser.webRequest.onBeforeRequest.addListener(
      async details => {
        browser.test.log("webRequest onBeforeRequest");
        let isRedirect =
          details.originUrl == browser.runtime.getURL("redirect.html") &&
          details.url.endsWith("print_postdata.sjs");
        let url = this.extUrl ? this.extUrl : details.url + "?redirected";
        return isRedirect ? { redirectUrl: url } : {};
      },
      { urls: ["*://*/*"] },
      ["blocking"]
    );
    browser.test.onMessage.addListener(async ({ method, url }) => {
      if (method == "setRedirectUrl") {
        this.extUrl = url;
      }
      browser.test.sendMessage("done");
    });
  },
};

async function withExtensionDummy(callback) {
  let extension = ExtensionTestUtils.loadExtension(EXTENSION_DATA);
  await extension.startup();
  let rv = await callback(`moz-extension://${extension.uuid}/`, extension);
  await extension.unload();
  return rv;
}

async function postFrom(start, target) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: start,
    },
    async function(browser) {
      info("Test tab ready: postFrom " + start);

      // Create the form element in our loaded URI.
      await SpecialPowers.spawn(browser, [{ target }], function({ target }) {
        // eslint-disable-next-line no-unsanitized/property
        content.document.body.innerHTML = `
        <form method="post" action="${target}">
          <input type="text" name="initialRemoteType" value="${Services.appinfo.remoteType}">
          <input type="submit" id="submit">
        </form>`;
      });

      // Perform a form POST submit load.
      info("Performing POST submission");
      await performLoad(
        browser,
        {
          url(url) {
            let enable =
              url.startsWith(PRINT_POSTDATA) ||
              url == target ||
              url == DATA_URL;
            if (!enable) {
              info(`url ${url} is invalid to perform load`);
            }
            return enable;
          },
          maybeErrorPage: true,
        },
        async () => {
          await SpecialPowers.spawn(browser, [], () => {
            content.document.querySelector("#submit").click();
          });
        }
      );

      // Check that the POST data was submitted.
      info("Fetching results");
      return SpecialPowers.spawn(browser, [], () => {
        return {
          remoteType: Services.appinfo.remoteType,
          location: "" + content.location.href,
          body: content.document.body.textContent,
        };
      });
    }
  );
}

async function loadAndGetProcessID(browser, target) {
  info(`Performing GET load: ${target}`);
  await performLoad(
    browser,
    {
      maybeErrorPage: true,
    },
    () => {
      BrowserTestUtils.loadURI(browser, target);
    }
  );

  info(`Navigated to: ${target}`);
  browser = gBrowser.selectedBrowser;
  let processID = await SpecialPowers.spawn(browser, [], () => {
    return Services.appinfo.processID;
  });
  return processID;
}

async function testLoadAndRedirect(
  target,
  expectedProcessSwitch,
  testRedirect
) {
  let start = httpURL(`dummy_page.html`);
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: start,
    },
    async function(_browser) {
      info("Test tab ready: getFrom " + start);

      let browser = gBrowser.selectedBrowser;
      let firstProcessID = await SpecialPowers.spawn(browser, [], () => {
        return Services.appinfo.processID;
      });

      info(`firstProcessID: ${firstProcessID}`);

      let secondProcessID = await loadAndGetProcessID(browser, target);

      info(`secondProcessID: ${secondProcessID}`);
      Assert.equal(firstProcessID != secondProcessID, expectedProcessSwitch);

      if (!testRedirect) {
        return;
      }

      let thirdProcessID = await loadAndGetProcessID(browser, add307(target));

      info(`thirdProcessID: ${thirdProcessID}`);
      Assert.equal(firstProcessID != thirdProcessID, expectedProcessSwitch);
      Assert.ok(secondProcessID == thirdProcessID);
    }
  );
}

add_task(async function test_enabled() {
  // URIs should correctly switch processes & the POST
  // should succeed.
  info("ENABLED -- FILE -- raw URI load");
  let resp = await postFrom(FILE_DUMMY, PRINT_POSTDATA);
  ok(E10SUtils.isWebRemoteType(resp.remoteType), "process switch");
  is(resp.location, PRINT_POSTDATA, "correct location");
  is(resp.body, "initialRemoteType=file", "correct POST body");

  info("ENABLED -- FILE -- 307-redirect URI load");
  let resp307 = await postFrom(FILE_DUMMY, add307(PRINT_POSTDATA));
  ok(E10SUtils.isWebRemoteType(resp307.remoteType), "process switch");
  is(resp307.location, PRINT_POSTDATA, "correct location");
  is(resp307.body, "initialRemoteType=file", "correct POST body");

  // Same with extensions
  await withExtensionDummy(async extOrigin => {
    info("ENABLED -- EXTENSION -- raw URI load");
    let respExt = await postFrom(extOrigin + "dummy.html", PRINT_POSTDATA);
    ok(E10SUtils.isWebRemoteType(respExt.remoteType), "process switch");
    is(respExt.location, PRINT_POSTDATA, "correct location");
    is(respExt.body, "initialRemoteType=extension", "correct POST body");

    info("ENABLED -- EXTENSION -- extension-redirect URI load");
    let respExtRedirect = await postFrom(
      extOrigin + "redirect.html",
      PRINT_POSTDATA
    );
    ok(E10SUtils.isWebRemoteType(respExtRedirect.remoteType), "process switch");
    is(
      respExtRedirect.location,
      PRINT_POSTDATA + "?redirected",
      "correct location"
    );
    is(
      respExtRedirect.body,
      "initialRemoteType=extension?redirected",
      "correct POST body"
    );

    info("ENABLED -- EXTENSION -- 307-redirect URI load");
    let respExt307 = await postFrom(
      extOrigin + "dummy.html",
      add307(PRINT_POSTDATA)
    );
    ok(E10SUtils.isWebRemoteType(respExt307.remoteType), "process switch");
    is(respExt307.location, PRINT_POSTDATA, "correct location");
    is(respExt307.body, "initialRemoteType=extension", "correct POST body");
  });
});

async function sendMessage(ext, method, url) {
  ext.sendMessage({ method, url });
  await ext.awaitMessage("done");
}

// TODO: Currently no test framework for ftp://.
add_task(async function test_protocol() {
  // TODO: Processes should be switched due to navigation of different origins.
  await testLoadAndRedirect("data:,foo", false, true);

  // Redirecting to file:// is not allowed.
  await testLoadAndRedirect(FILE_DUMMY, true, false);

  await withExtensionDummy(async (extOrigin, extension) => {
    await sendMessage(extension, "setRedirectUrl", DATA_URL);

    let respExtRedirect = await postFrom(
      extOrigin + "redirect.html",
      PRINT_POSTDATA
    );

    // TODO: Processes should be switched due to navigation of different origins.
    is(respExtRedirect.remoteType, "extension", "process switch");
    is(respExtRedirect.location, DATA_URL, "correct location");
    is(respExtRedirect.body, DATA_STRING, "correct POST body");
  });
});
