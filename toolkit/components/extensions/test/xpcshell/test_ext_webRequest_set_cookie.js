"use strict";

const HOSTS = new Set(["example.com"]);

const server = createHttpServer({ hosts: HOSTS });

server.registerDirectory("/data/", do_get_file("data"));

server.registerPathHandler(
  "/file_webrequestblocking_set_cookie.html",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Set-Cookie", "reqcookie=reqvalue", false);
    response.write("<!DOCTYPE html><html></html>");
  }
);

add_task(async function test_modifying_cookies_from_onHeadersReceived() {
  async function background() {
    /**
     * Check that all the cookies described by `prefixes` are in the cookie jar.
     *
     * @param {Array.string} prefixes
     *   Zero or more prefixes, describing cookies that are expected to be set
     *   in the current cookie jar.  Each prefix describes both a cookie
     *   name and corresponding value.  For example, if the string "ext"
     *   is passed as an argument, then this function expects to see
     *   a cookie called "extcookie" and corresponding value of "extvalue".
     */
    async function checkCookies(prefixes) {
      const numPrefixes = prefixes.length;
      const currentCookies = await browser.cookies.getAll({});
      browser.test.assertEq(
        numPrefixes,
        currentCookies.length,
        `${numPrefixes} cookies were set`
      );

      for (let cookiePrefix of prefixes) {
        let cookieName = `${cookiePrefix}cookie`;
        let expectedCookieValue = `${cookiePrefix}value`;
        let fetchedCookie = await browser.cookies.getAll({ name: cookieName });
        browser.test.assertEq(
          1,
          fetchedCookie.length,
          `Found 1 cookie with name "${cookieName}"`
        );
        browser.test.assertEq(
          expectedCookieValue,
          fetchedCookie[0] && fetchedCookie[0].value,
          `Cookie "${cookieName}" has expected value of "${expectedCookieValue}"`
        );
      }
    }

    function awaitMessage(expectedMsg) {
      return new Promise(resolve => {
        browser.test.onMessage.addListener(function listener(msg) {
          if (msg === expectedMsg) {
            browser.test.onMessage.removeListener(listener);
            resolve();
          }
        });
      });
    }

    /**
     * Opens the given test file as a content page.
     *
     * @param {string} filename
     *   The name of a html file relative to the test server root.
     *
     * @returns {Promise}
     */
    function openContentPage(filename) {
      let promise = awaitMessage("url-loaded");
      browser.test.sendMessage(
        "load-url",
        `http://example.com/${filename}?nocache=${Math.random()}`
      );
      return promise;
    }

    /**
     * Tests that expected cookies are in the cookie jar after opening a file.
     *
     * @param {string} filename
     *   The name of a html file in the
     *   "toolkit/components/extensions/test/mochitest" directory.
     * @param {?Array.string} prefixes
     *   Zero or more prefixes, describing cookies that are expected to be set
     *   in the current cookie jar.  Each prefix describes both a cookie
     *   name and corresponding value.  For example, if the string "ext"
     *   is passed as an argument, then this function expects to see
     *   a cookie called "extcookie" and corresponding value of "extvalue".
     *   If undefined, then no checks are automatically performed, and the
     *   caller should provide a callback to perform the checks.
     * @param {?Function} callback
     *   An optional async callback function that, if provided, will be called
     *   with an object that contains windowId and tabId parameters.
     *   Callers can use this callback to apply extra tests about the state of
     *   the cookie jar, or to query the state of the opened page.
     */
    async function testCookiesWithFile(filename, prefixes, callback) {
      await browser.browsingData.removeCookies({});
      await openContentPage(filename);

      if (prefixes !== undefined) {
        await checkCookies(prefixes);
      }

      if (callback !== undefined) {
        await callback();
      }
      let promise = awaitMessage("url-unloaded");
      browser.test.sendMessage("unload-url");
      await promise;
    }

    const filter = {
      urls: ["<all_urls>"],
      types: ["main_frame", "sub_frame"],
    };

    const headersReceivedInfoSpec = ["blocking", "responseHeaders"];

    const onHeadersReceived = details => {
      details.responseHeaders.push({
        name: "Set-Cookie",
        value: "extcookie=extvalue",
      });

      return {
        responseHeaders: details.responseHeaders,
      };
    };
    browser.webRequest.onHeadersReceived.addListener(
      onHeadersReceived,
      filter,
      headersReceivedInfoSpec
    );

    // First, perform a request that should not set any cookies, and check
    // that the cookie the extension sets is the only cookie in the
    // cookie jar.
    await testCookiesWithFile("data/file_sample.html", ["ext"]);

    // Next, perform a request that will set on cookie (reqcookie=reqvalue)
    // and check that two cookies wind up in the cookie jar (the request
    // set cookie, and the extension set cookie).
    await testCookiesWithFile("file_webrequestblocking_set_cookie.html", [
      "ext",
      "req",
    ]);

    // Third, register another onHeadersReceived handler that also
    // sets a cookie (thirdcookie=thirdvalue), to make sure modifications from
    // multiple onHeadersReceived listeners are merged correctly.
    const thirdOnHeadersRecievedListener = details => {
      details.responseHeaders.push({
        name: "Set-Cookie",
        value: "thirdcookie=thirdvalue",
      });

      browser.test.log(JSON.stringify(details.responseHeaders));

      return {
        responseHeaders: details.responseHeaders,
      };
    };
    browser.webRequest.onHeadersReceived.addListener(
      thirdOnHeadersRecievedListener,
      filter,
      headersReceivedInfoSpec
    );
    await testCookiesWithFile("file_webrequestblocking_set_cookie.html", [
      "ext",
      "req",
      "third",
    ]);
    browser.webRequest.onHeadersReceived.removeListener(onHeadersReceived);
    browser.webRequest.onHeadersReceived.removeListener(
      thirdOnHeadersRecievedListener
    );

    // Fourth, test to make sure that extensions can remove cookies
    // using onHeadersReceived too, by 1. making a request that
    // sets a cookie (reqcookie=reqvalue), 2. having the extension remove
    // that cookie by removing that header, and 3. adding a new cookie
    // (extcookie=extvalue).
    const fourthOnHeadersRecievedListener = details => {
      // Remove the cookie set by the request (reqcookie=reqvalue).
      const newHeaders = details.responseHeaders.filter(
        cookie => cookie.name !== "set-cookie"
      );

      // And then add a new cookie in its place (extcookie=extvalue).
      newHeaders.push({
        name: "Set-Cookie",
        value: "extcookie=extvalue",
      });

      return {
        responseHeaders: newHeaders,
      };
    };
    browser.webRequest.onHeadersReceived.addListener(
      fourthOnHeadersRecievedListener,
      filter,
      headersReceivedInfoSpec
    );
    await testCookiesWithFile("file_webrequestblocking_set_cookie.html", [
      "ext",
    ]);
    browser.webRequest.onHeadersReceived.removeListener(
      fourthOnHeadersRecievedListener
    );

    // Fifth, check that extensions are able to overwrite headers set by
    // pages. In this test, make a request that will set "reqcookie=reqvalue",
    // and add a listener that sets "reqcookie=changedvalue".  Check
    // to make sure that the cookie jar contains "reqcookie=changedvalue"
    // and not "reqcookie=reqvalue".
    const fifthOnHeadersRecievedListener = details => {
      // Remove the cookie set by the request (reqcookie=reqvalue).
      const newHeaders = details.responseHeaders.filter(
        cookie => cookie.name !== "set-cookie"
      );

      // And then add a new cookie in its place (reqcookie=changedvalue).
      newHeaders.push({
        name: "Set-Cookie",
        value: "reqcookie=changedvalue",
      });

      return {
        responseHeaders: newHeaders,
      };
    };
    browser.webRequest.onHeadersReceived.addListener(
      fifthOnHeadersRecievedListener,
      filter,
      headersReceivedInfoSpec
    );

    await testCookiesWithFile(
      "file_webrequestblocking_set_cookie.html",
      undefined,
      async () => {
        const currentCookies = await browser.cookies.getAll({});
        browser.test.assertEq(1, currentCookies.length, `1 cookie was set`);

        const cookieName = "reqcookie";
        const expectedCookieValue = "changedvalue";
        const fetchedCookie = await browser.cookies.getAll({
          name: cookieName,
        });

        browser.test.assertEq(
          1,
          fetchedCookie.length,
          `Found 1 cookie with name "${cookieName}"`
        );
        browser.test.assertEq(
          expectedCookieValue,
          fetchedCookie[0] && fetchedCookie[0].value,
          `Cookie "${cookieName}" has expected value of "${expectedCookieValue}"`
        );
      }
    );
    browser.webRequest.onHeadersReceived.removeListener(
      fifthOnHeadersRecievedListener
    );

    browser.test.notifyPass("cookie modifying extension");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "browsingData",
        "cookies",
        "webNavigation",
        "webRequest",
        "webRequestBlocking",
        "<all_urls>",
      ],
    },
    background,
  });

  let contentPage = null;
  extension.onMessage("load-url", async url => {
    ok(!contentPage, "Should have no content page to unload");
    contentPage = await ExtensionTestUtils.loadContentPage(url);
    extension.sendMessage("url-loaded");
  });
  extension.onMessage("unload-url", async () => {
    await contentPage.close();
    contentPage = null;
    extension.sendMessage("url-unloaded");
  });

  await extension.startup();
  await extension.awaitFinish("cookie modifying extension");
  await extension.unload();
});
