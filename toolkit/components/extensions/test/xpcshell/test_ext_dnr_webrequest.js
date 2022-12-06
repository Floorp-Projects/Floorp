"use strict";

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
});

const server = createHttpServer({
  hosts: ["example.com", "redir"],
});
server.registerPathHandler("/never_reached", (req, res) => {
  Assert.ok(false, "Server should never have been reached");
});
server.registerPathHandler("/source", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
});
server.registerPathHandler("/destination", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
});

add_task(async function block_request_with_dnr() {
  async function background() {
    let onBeforeRequestPromise = new Promise(resolve => {
      browser.webRequest.onBeforeRequest.addListener(resolve, {
        urls: ["*://example.com/*"],
      });
    });
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [
        {
          id: 1,
          condition: { requestDomains: ["example.com"] },
          action: { type: "block" },
        },
      ],
    });

    await browser.test.assertRejects(
      fetch("http://example.com/never_reached"),
      "NetworkError when attempting to fetch resource.",
      "blocked by DNR rule"
    );
    // DNR is documented to take precedence over webRequest. We should still
    // receive the webRequest event, however.
    browser.test.log("Waiting for webRequest.onBeforeRequest...");
    await onBeforeRequestPromise;
    browser.test.log("Seen webRequest.onBeforeRequest!");

    browser.test.notifyPass();
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    temporarilyInstalled: true, // Needed for granted_host_permissions
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      granted_host_permissions: true,
      host_permissions: ["*://example.com/*"],
      permissions: ["declarativeNetRequest", "webRequest"],
    },
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function upgradeScheme_and_redirect_request_with_dnr() {
  async function background() {
    let onBeforeRequestSeen = [];
    browser.webRequest.onBeforeRequest.addListener(
      d => {
        onBeforeRequestSeen.push(d.url);
        // webRequest cancels, but DNR should actually be taking precedence.
        return { cancel: true };
      },
      { urls: ["*://example.com/*", "http://redir/here"] },
      ["blocking"]
    );
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [
        {
          id: 1,
          condition: { requestDomains: ["example.com"] },
          action: { type: "upgradeScheme" },
        },
        {
          id: 2,
          condition: { requestDomains: ["example.com"], urlFilter: "|https:*" },
          action: { type: "redirect", redirect: { url: "http://redir/here" } },
          // The upgradeScheme and redirect actions have equal precedence. To
          // make sure that the redirect action is executed when both rules
          // match, we assign a higher priority to the redirect action.
          priority: 2,
        },
      ],
    });

    await browser.test.assertRejects(
      fetch("http://example.com/never_reached"),
      "NetworkError when attempting to fetch resource.",
      "although initially redirected by DNR, ultimately blocked by webRequest"
    );
    // DNR is documented to take precedence over webRequest.
    // So we should actually see redirects according to the DNR rules, and
    // the webRequest listener should still be able to observe all requests.
    browser.test.assertDeepEq(
      [
        "http://example.com/never_reached",
        "https://example.com/never_reached",
        "http://redir/here",
      ],
      onBeforeRequestSeen,
      "Expected onBeforeRequest events"
    );

    browser.test.notifyPass();
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    temporarilyInstalled: true, // Needed for granted_host_permissions
    manifest: {
      manifest_version: 3,
      granted_host_permissions: true,
      host_permissions: ["*://example.com/*", "*://redir/*"],
      permissions: [
        "declarativeNetRequest",
        "webRequest",
        "webRequestBlocking",
      ],
    },
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function block_request_with_webRequest_after_allow_with_dnr() {
  async function background() {
    let onBeforeRequestSeen = [];
    browser.webRequest.onBeforeRequest.addListener(
      d => {
        onBeforeRequestSeen.push(d.url);
        return { cancel: !d.url.includes("webRequestNoCancel") };
      },
      { urls: ["*://example.com/*"] },
      ["blocking"]
    );
    // All DNR actions that do not end up canceling/redirecting the request:
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [
        {
          id: 1,
          condition: { requestMethods: ["get"] },
          action: { type: "allow" },
        },
        {
          id: 2,
          condition: { requestMethods: ["put"] },
          action: {
            type: "modifyHeaders",
            requestHeaders: [{ operation: "set", header: "x", value: "y" }],
          },
        },
      ],
    });

    await browser.test.assertRejects(
      fetch("http://example.com/never_reached?1", { method: "get" }),
      "NetworkError when attempting to fetch resource.",
      "despite DNR 'allow' rule, still blocked by webRequest"
    );
    await browser.test.assertRejects(
      fetch("http://example.com/never_reached?2", { method: "put" }),
      "NetworkError when attempting to fetch resource.",
      "despite DNR 'modifyHeaders' rule, still blocked by webRequest"
    );
    // Just to rule out the request having been canceled by DNR instead of
    // webRequest, repeat the requests and verify that they succeed.
    await fetch("http://example.com/?webRequestNoCancel1", { method: "get" });
    await fetch("http://example.com/?webRequestNoCancel2", { method: "put" });

    browser.test.assertDeepEq(
      [
        "http://example.com/never_reached?1",
        "http://example.com/never_reached?2",
        "http://example.com/?webRequestNoCancel1",
        "http://example.com/?webRequestNoCancel2",
      ],
      onBeforeRequestSeen,
      "Expected onBeforeRequest events"
    );

    browser.test.notifyPass();
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    temporarilyInstalled: true, // Needed for granted_host_permissions
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      granted_host_permissions: true,
      host_permissions: ["*://example.com/*"],
      permissions: [
        "declarativeNetRequest",
        "webRequest",
        "webRequestBlocking",
      ],
    },
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function redirect_with_webRequest_after_failing_dnr_redirect() {
  async function background() {
    // Maximum length of a UTL is 1048576 (network.standard-url.max-length).
    const network_standard_url_max_length = 1048576;
    // updateSessionRules does some validation on the limit (as seen by
    // validate_action_redirect_transform in test_ext_dnr_session_rules.js),
    // but it is still possible to pass validation and fail in practice when
    // the existing URL + new component exceeds the limit.
    const VERY_LONG_STRING = "x".repeat(network_standard_url_max_length - 20);

    browser.webRequest.onBeforeRequest.addListener(
      d => {
        return { redirectUrl: "http://redir/destination?by-webrequest" };
      },
      { urls: ["*://example.com/*"] },
      ["blocking"]
    );
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [
        {
          id: 1,
          condition: { requestDomains: ["example.com"] },
          action: {
            type: "redirect",
            redirect: {
              transform: {
                host: "redir",
                path: "/destination",
                queryTransform: {
                  addOrReplaceParams: [
                    { key: "dnr", value: VERY_LONG_STRING, replaceOnly: true },
                  ],
                },
              },
            },
          },
        },
      ],
    });

    // Note: we are not expecting successful DNR redirects below, but in case
    // that ever changes (e.g. due to VERY_LONG_STRING not resulting in an
    // invalid URL), we will truncate the URL out of caution.
    // VERY_LONG_STRING consists of many 'X'. Shorten to avoid logspam.
    const shortx = s => s.replace(/x{10,}/g, xxx => `x{${xxx.length}}`);

    browser.test.assertEq(
      "http://redir/destination?1",
      shortx((await fetch("http://example.com/never_reached?1")).url),
      "Successful DNR redirect."
    );

    // DNR redirect failure is expected to be very rare, and only to occur when
    // an extension intentionally explores the boundaries of the DNR API. When
    // DNR fails, we fall back to allowing webRequest to take over.
    browser.test.assertEq(
      "http://redir/destination?by-webrequest",
      shortx((await fetch("http://example.com/source?dnr")).url),
      "When DNR fails, we fall back to webRequest redirect"
    );

    browser.test.notifyPass();
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    temporarilyInstalled: true, // Needed for granted_host_permissions
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      granted_host_permissions: true,
      host_permissions: ["*://example.com/*"],
      permissions: [
        "declarativeNetRequest",
        "webRequest",
        "webRequestBlocking",
      ],
    },
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});
