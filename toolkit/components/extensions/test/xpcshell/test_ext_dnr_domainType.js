/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr.js")).spec,
  this
);

const server = createHttpServer({
  hosts: ["example.com", "example.org", "sub.example.com", "sub.example.org"],
});

const BASE_COM = "http://example.com";
const BASE_COM_SUB = "http://sub.example.com";
const BASE_COM_XHR = `${BASE_COM}/xhr`;
const BASE_ORG = "http://example.org";

// Small red image.
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

const testServerHandlers = [
  {
    path: "/",
    contentType: "text/html",
    responseBody: "<!DOCTYPE html><html></html>",
  },
  {
    path: "/xhr",
    contentType: "text/json",
    responseBody: JSON.stringify({ success: true }),
    allowCORS: true,
  },
  {
    path: "/image.png",
    contentType: "image/png",
    responseBody: IMG_BYTES,
  },
  {
    path: "/script.js",
    contentType: "text/javascript",
    responseBody: "",
  },
];

for (const handler of testServerHandlers) {
  const { path, contentType, responseBody, allowCORS } = handler;
  server.registerPathHandler(path, (req, res) => {
    res.setStatusLine(req.httpVersion, 200, "OK");
    res.setHeader("Content-Type", contentType, false);
    if (allowCORS) {
      res.setHeader("Access-Control-Allow-Origin", "*");
      res.setHeader("Access-Control-Max-Age", "0");
    }
    res.write(responseBody);
  });
}

function getDNRTestExtension() {
  function background() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      let result;
      let createRejectionHandler = apiName => err => {
        browser.test.fail(`Got unexpected error from ${apiName}: ${err}`);
      };
      switch (msg) {
        case "updateSessionRules": {
          await browser.declarativeNetRequest
            .updateSessionRules(args[0])
            .catch(createRejectionHandler("updateSessionRules"));
          break;
        }
        default:
          browser.test.fail(`Got unexpected test message: ${msg}`);
      }
      browser.test.sendMessage(`${msg}:done`, result);
    });
    browser.test.sendMessage("bg:ready");
  }

  return {
    background,
    manifest: {
      permissions: ["declarativeNetRequest"],
    },
  };
}

async function updateSessionRules(extension, opts) {
  await extension.sendMessage("updateSessionRules", opts);
  await extension.awaitMessage("updateSessionRules:done");
}

function promiseHttpOnStopRequest(url, msg = `request ${url}`) {
  return TestUtils.topicObserved("http-on-stop-request", s => {
    const chan = s.QueryInterface(Ci.nsIHttpChannel);
    return chan.URI.spec === url;
  }).then(([chan]) => {
    if (!chan.canceled) {
      const { responseStatus } = chan;
      info(
        `Got http-on-stop-request for ${msg}, responseStatus ${responseStatus}`
      );
      return { responseStatus };
    }
    let blockingReason = chan.loadInfo.requestBlockingReason;
    info(
      `Got http-on-stop-request for ${msg}, blockingReason ${blockingReason}`
    );
    let addonId;
    try {
      const props = chan.QueryInterface(Ci.nsIPropertyBag);
      addonId = props.getProperty("cancelledByExtension");
    } catch (err) {
      // Channel may not have a property bag and chan.QueryInterface.
    }
    return { addonId, blockingReason };
  });
}

add_task(async function test_request_domain_type_xhr() {
  const extension = ExtensionTestUtils.loadExtension(getDNRTestExtension());
  await extension.startup();
  await extension.awaitMessage("bg:ready");

  const testXHR = (origin, url, { expectBlocked = false } = {}) => {
    const msg = `XHR request ${url} from ${origin}`;
    info(`Test ${msg}`);
    const promiseRequestStop = promiseHttpOnStopRequest(url, msg).then(
      result => {
        const blockingReason =
          Ci.nsILoadInfo.BLOCKING_REASON_EXTENSION_WEBREQUEST;
        const expectedResult = expectBlocked
          ? { addonId: extension.id, blockingReason }
          : { responseStatus: 200 };
        Assert.deepEqual(
          result,
          expectedResult,
          `Got the expected result for ${msg} http-on-stop-request`
        );
      }
    );
    const promiseFetch = ExtensionTestUtils.fetch(origin, url).catch(err => {
      if (expectBlocked) {
        Assert.ok(
          /NetworkError/.test(err.message),
          `${msg} got rejected as expected: ${err}`
        );
      } else {
        Assert.ok(
          false,
          `${msg} expected to succeeded but for rejected: ${err}`
        );
      }
    });
    return Promise.all([promiseFetch, promiseRequestStop]);
  };

  // Sanity checks.
  info("Trigger sanity check requests with no active DNR rules");
  await testXHR(BASE_COM, BASE_COM_XHR, { expectBlocked: false });
  await testXHR(BASE_ORG, BASE_COM_XHR, { expectBlocked: false });
  await testXHR(BASE_COM_SUB, BASE_COM_XHR, { expectBlocked: false });

  info("Update DNR session rules to block firstParty XHR requests");
  const blockFirstPartyRule = getDNRRule({
    id: 1,
    action: { type: "block" },
    condition: {
      domainType: "firstParty",
      resourceTypes: ["xmlhttprequest"],
      urlFilter: `|${BASE_COM_XHR}`,
    },
  });
  await updateSessionRules(extension, { addRules: [blockFirstPartyRule] });
  await testXHR(BASE_COM, BASE_COM_XHR, { expectBlocked: true });
  await testXHR(BASE_COM_SUB, BASE_COM_XHR, { expectBlocked: true });
  await testXHR(BASE_ORG, BASE_COM_XHR, { expectBlocked: false });

  info("Update DNR session rules to block thirdParty XHR requests");
  const blockThirdPartyRule = getDNRRule({
    id: 2,
    action: { type: "block" },
    condition: {
      domainType: "thirdParty",
      resourceTypes: ["xmlhttprequest"],
      urlFilter: `|${BASE_COM_XHR}`,
    },
  });
  await updateSessionRules(extension, {
    addRules: [blockThirdPartyRule],
    removeRuleIds: [blockFirstPartyRule.id],
  });
  await testXHR(BASE_COM, BASE_COM_XHR, { expectBlocked: false });
  await testXHR(BASE_COM_SUB, BASE_COM_XHR, { expectBlocked: false });
  await testXHR(BASE_ORG, BASE_COM_XHR, { expectBlocked: true });

  await extension.unload();
});

add_task(async function test_request_domain_type_subresources() {
  const extension = ExtensionTestUtils.loadExtension(getDNRTestExtension());
  await extension.startup();
  await extension.awaitMessage("bg:ready");

  // Test helper used to test subresource requests triggered by
  // img and script tags.
  const testTag = (tagName, targetPage, targetUrl, expectBlocked) => {
    info(`==== Test ${tagName} request to ${targetUrl}`);
    const promiseRequestStop = promiseHttpOnStopRequest(targetUrl).then(
      result => {
        const blockingReason =
          Ci.nsILoadInfo.BLOCKING_REASON_EXTENSION_WEBREQUEST;
        const expectedResult = expectBlocked
          ? { addonId: extension.id, blockingReason }
          : { responseStatus: 200 };
        Assert.deepEqual(
          result,
          expectedResult,
          `Got the expected result for ${targetUrl} http-on-stop-request`
        );
      }
    );
    const promiseSpawn = targetPage.spawn([tagName, targetUrl], (tag, url) => {
      const doc = this.content.document;
      const el = doc.createElement(tag);
      el.src = url;
      doc.body.appendChild(el);
    });
    return Promise.all([promiseRequestStop, promiseSpawn]);
  };

  const testSubResourcesRequests = async (pageUrl, testCases) => {
    let page = await ExtensionTestUtils.loadContentPage(pageUrl);
    for (const testCase of testCases) {
      const { tag, url, expectBlocked } = testCase;
      await testTag(tag, page, url, expectBlocked);
    }
    await page.close();
  };

  // Sanity check
  await testSubResourcesRequests(BASE_COM, [
    { tag: "img", url: `${BASE_COM}/image.png`, expectBlocked: false },
    {
      tag: "img",
      url: `${BASE_COM_SUB}/image.png`,
      expectBlocked: false,
    },
    { tag: "img", url: `${BASE_ORG}/image.png`, expectBlocked: false },
    { tag: "script", url: `${BASE_COM}/script.js`, expectBlocked: false },
    {
      tag: "script",
      url: `${BASE_COM_SUB}/script.js`,
      expectBlocked: false,
    },
    { tag: "script", url: `${BASE_ORG}/script.js`, expectBlocked: false },
    { tag: "iframe", url: `${BASE_COM}/`, expectBlocked: false },
    { tag: "iframe", url: `${BASE_COM_SUB}/`, expectBlocked: false },
    { tag: "iframe", url: `${BASE_ORG}/`, expectBlocked: false },
  ]);

  info("Update DNR session rules to block first party image requests");
  const blockFirstPartyRule = getDNRRule({
    id: 1,
    action: { type: "block" },
    condition: {
      domainType: "firstParty",
      resourceTypes: ["image", "script", "sub_frame"],
    },
  });
  await updateSessionRules(extension, { addRules: [blockFirstPartyRule] });
  await testSubResourcesRequests(BASE_COM, [
    { tag: "img", url: `${BASE_COM}/image.png`, expectBlocked: true },
    {
      tag: "img",
      url: `${BASE_COM_SUB}/image.png`,
      expectBlocked: true,
    },
    { tag: "img", url: `${BASE_ORG}/image.png`, expectBlocked: false },
    { tag: "script", url: `${BASE_COM}/script.js`, expectBlocked: true },
    {
      tag: "script",
      url: `${BASE_COM_SUB}/script.js`,
      expectBlocked: true,
    },
    { tag: "script", url: `${BASE_ORG}/script.js`, expectBlocked: false },
    { tag: "iframe", url: `${BASE_COM}/`, expectBlocked: true },
    { tag: "iframe", url: `${BASE_COM_SUB}/`, expectBlocked: true },
    { tag: "iframe", url: `${BASE_ORG}/`, expectBlocked: false },
  ]);

  info("Update DNR session rules to block third party image requests");
  const blockThirdPartyRule = getDNRRule({
    id: 2,
    action: { type: "block" },
    condition: {
      domainType: "thirdParty",
      resourceTypes: ["image", "script", "sub_frame"],
    },
  });
  await updateSessionRules(extension, {
    addRules: [blockThirdPartyRule],
    removeRuleIds: [blockFirstPartyRule.id],
  });
  await testSubResourcesRequests(BASE_COM, [
    { tag: "img", url: `${BASE_COM}/image.png`, expectBlocked: false },
    { tag: "img", url: `${BASE_COM_SUB}/image.png`, expectBlocked: false },
    { tag: "img", url: `${BASE_ORG}/image.png`, expectBlocked: true },
    { tag: "script", url: `${BASE_COM}/script.js`, expectBlocked: false },
    {
      tag: "script",
      url: `${BASE_COM_SUB}/script.js`,
      expectBlocked: false,
    },
    { tag: "script", url: `${BASE_ORG}/script.js`, expectBlocked: true },
    { tag: "iframe", url: `${BASE_COM}/`, expectBlocked: false },
    { tag: "iframe", url: `${BASE_COM_SUB}/`, expectBlocked: false },
    { tag: "iframe", url: `${BASE_ORG}/`, expectBlocked: true },
  ]);

  await extension.unload();
});
