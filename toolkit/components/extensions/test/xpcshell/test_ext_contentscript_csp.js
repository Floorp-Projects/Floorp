/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const server = createHttpServer({
  hosts: ["example.com", "csplog.example.net"],
});
server.registerDirectory("/data/", do_get_file("data"));

var gDefaultCSP = `default-src 'self' 'report-sample'; script-src 'self' 'report-sample';`;
var gCSP = gDefaultCSP;
const pageContent = `<!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <title></title>
  </head>
  <body>
  <img id="testimg">
  </body>
  </html>`;

server.registerPathHandler("/plain.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html");
  if (gCSP) {
    info(`Content-Security-Policy: ${gCSP}`);
    response.setHeader("Content-Security-Policy", gCSP);
  }
  response.write(pageContent);
});

const BASE_URL = `http://example.com`;
const pageURL = `${BASE_URL}/plain.html`;

const CSP_REPORT_PATH = "/csp-report.sjs";

function readUTF8InputStream(stream) {
  let buffer = NetUtil.readInputStream(stream, stream.available());
  return new TextDecoder().decode(buffer);
}

server.registerPathHandler(CSP_REPORT_PATH, (request, response) => {
  response.setStatusLine(request.httpVersion, 204, "No Content");
  let data = readUTF8InputStream(request.bodyInputStream);
  Services.obs.notifyObservers(null, "extension-test-csp-report", data);
});

async function promiseCSPReport(test) {
  let res = await TestUtils.topicObserved("extension-test-csp-report", test);
  return JSON.parse(res[1]);
}

// Test functions loaded into extension content script.
function testImage(data = {}) {
  return new Promise(resolve => {
    let img = window.document.getElementById("testimg");
    img.onload = () => resolve(true);
    img.onerror = () => {
      browser.test.log(`img error: ${img.src}`);
      resolve(false);
    };
    img.src = data.image_url;
  });
}

function testFetch(data = {}) {
  let f = data.content ? content.fetch : fetch;
  return f(data.url)
    .then(() => true)
    .catch(e => {
      browser.test.assertEq(
        e.message,
        "NetworkError when attempting to fetch resource.",
        "expected fetch failure"
      );
      return false;
    });
}

async function testEval(data = {}) {
  try {
    // eslint-disable-next-line no-eval
    let ev = data.content ? window.eval : eval;
    return ev("true");
  } catch (e) {
    return false;
  }
}

async function testFunction(data = {}) {
  try {
    // eslint-disable-next-line no-eval
    let fn = data.content ? window.Function : Function;
    let sum = new fn("a", "b", "return a + b");
    return sum(1, 1);
  } catch (e) {
    return 0;
  }
}

function testScriptTag(data) {
  return new Promise(resolve => {
    let script = document.createElement("script");
    script.src = data.url;
    script.onload = () => {
      resolve(true);
    };
    script.onerror = () => {
      resolve(false);
    };
    document.body.appendChild(script);
  });
}

async function testHttpRequestUpgraded(data = {}) {
  let f = data.content ? content.fetch : fetch;
  return f(data.url)
    .then(() => "http:")
    .catch(() => "https:");
}

async function testWebSocketUpgraded(data = {}) {
  let ws = data.content ? content.WebSocket : WebSocket;
  new ws(data.url);
}

function webSocketUpgradeListenerBackground() {
  // Catch websocket requests and send the protocol back to be asserted.
  browser.webRequest.onBeforeRequest.addListener(
    details => {
      // Send the protocol back as test result.
      // This will either be "wss:", "ws:"
      browser.test.sendMessage("result", new URL(details.url).protocol);
      return { cancel: true };
    },
    { urls: ["wss://example.com/*", "ws://example.com/*"] },
    ["blocking"]
  );
}

// If the violation source is the extension the securitypolicyviolation event is not fired.
// If the page is the source, the event is fired and both the content script or page scripts
// will receive the event.  If we're expecting a moz-extension report  we'll  fail in the
// event listener if we receive a report.  Otherwise we want to resolve in the listener to
// ensure we've received the event for the test.
function contentScript(report) {
  return new Promise(resolve => {
    if (!report || report["document-uri"] === "moz-extension") {
      resolve();
    }
    // eslint-disable-next-line mozilla/balanced-listeners
    document.addEventListener("securitypolicyviolation", e => {
      browser.test.assertTrue(
        e.documentURI !== "moz-extension",
        `securitypolicyviolation: ${e.violatedDirective} ${e.documentURI}`
      );
      resolve();
    });
  });
}

let TESTS = [
  // Image Tests
  {
    description:
      "Image from content script using default extension csp. Image is allowed.",
    pageCSP: `${gDefaultCSP} img-src 'none';`,
    script: testImage,
    data: { image_url: `${BASE_URL}/data/file_image_good.png` },
    expect: true,
  },
  // Fetch Tests
  {
    description: "Fetch url in content script uses default extension csp.",
    pageCSP: `${gDefaultCSP} connect-src 'none';`,
    script: testFetch,
    data: { url: `${BASE_URL}/data/file_image_good.png` },
    expect: true,
  },
  {
    description: "Fetch full url from content script uses page csp.",
    pageCSP: `${gDefaultCSP} connect-src 'none';`,
    script: testFetch,
    data: {
      content: true,
      url: `${BASE_URL}/data/file_image_good.png`,
    },
    expect: false,
    report: {
      "blocked-uri": `${BASE_URL}/data/file_image_good.png`,
      "document-uri": `${BASE_URL}/plain.html`,
      "violated-directive": "connect-src",
    },
  },

  // Eval tests.
  {
    description: "Eval from content script uses page csp with unsafe-eval.",
    pageCSP: `default-src 'none'; script-src 'unsafe-eval';`,
    script: testEval,
    data: { content: true },
    expect: true,
  },
  {
    description: "Eval from content script uses page csp.",
    pageCSP: `default-src 'self' 'report-sample'; script-src 'self';`,
    version: 3,
    script: testEval,
    data: { content: true },
    expect: false,
    report: {
      "blocked-uri": "eval",
      "document-uri": "http://example.com/plain.html",
      "violated-directive": "script-src",
    },
  },
  {
    description: "Eval in content script allowed by v2 csp.",
    pageCSP: `script-src 'self' 'unsafe-eval';`,
    script: testEval,
    expect: true,
  },
  {
    description: "Eval in content script disallowed by v3 csp.",
    pageCSP: `script-src 'self' 'unsafe-eval';`,
    version: 3,
    script: testEval,
    expect: false,
  },
  {
    description: "Wrapped Eval in content script uses page csp.",
    pageCSP: `script-src 'self' 'unsafe-eval';`,
    version: 3,
    script: async () => {
      return window.wrappedJSObject.eval("true");
    },
    expect: true,
  },
  {
    description: "Wrapped Eval in content script denied by page csp.",
    pageCSP: `script-src 'self';`,
    version: 3,
    script: async () => {
      try {
        return window.wrappedJSObject.eval("true");
      } catch (e) {
        return false;
      }
    },
    expect: false,
  },

  {
    description: "Function from content script uses page csp.",
    pageCSP: `default-src 'self'; script-src 'self' 'unsafe-eval';`,
    script: testFunction,
    data: { content: true },
    expect: 2,
  },
  {
    description: "Function from content script uses page csp.",
    pageCSP: `default-src 'self' 'report-sample'; script-src 'self';`,
    version: 3,
    script: testFunction,
    data: { content: true },
    expect: 0,
    report: {
      "blocked-uri": "eval",
      "document-uri": "http://example.com/plain.html",
      "violated-directive": "script-src",
    },
  },
  {
    description: "Function in content script uses extension csp.",
    pageCSP: `default-src 'self'; script-src 'self' 'unsafe-eval';`,
    version: 3,
    script: testFunction,
    expect: 0,
  },

  // The javascript url tests are not included as we do not execute those,
  // aparently even with the urlbar filtering pref flipped.
  // (browser.urlbar.filter.javascript)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=866522

  // script tag injection tests
  {
    description: "remote script in content script passes in v2",
    version: 2,
    pageCSP: "script-src http://example.com:*;",
    script: testScriptTag,
    data: { url: `${BASE_URL}/data/file_script_good.js` },
    expect: true,
  },
  {
    description: "remote script in content script fails in v3",
    version: 3,
    pageCSP: "script-src http://example.com:*;",
    script: testScriptTag,
    data: { url: `${BASE_URL}/data/file_script_good.js` },
    expect: false,
  },
  {
    description: "content.WebSocket in content script is affected by page csp.",
    version: 2,
    pageCSP: `upgrade-insecure-requests;`,
    data: { content: true, url: "ws://example.com/ws_dummy" },
    script: testWebSocketUpgraded,
    expect: "wss:", // we expect the websocket to be upgraded.
    backgroundScript: webSocketUpgradeListenerBackground,
  },
  {
    description: "WebSocket in content script is not affected by page csp.",
    version: 2,
    pageCSP: `upgrade-insecure-requests;`,
    data: { url: "ws://example.com/ws_dummy" },
    script: testWebSocketUpgraded,
    expect: "ws:", // we expect the websocket to not be upgraded.
    backgroundScript: webSocketUpgradeListenerBackground,
  },
  {
    description: "WebSocket in content script is not affected by page csp. v3",
    version: 3,
    pageCSP: `upgrade-insecure-requests;`,
    data: { url: "ws://example.com/ws_dummy" },
    script: testWebSocketUpgraded,
    // TODO bug 1766813: MV3+WebSocket should use content script CSP.
    expect: "wss:", // TODO: we expect the websocket to not be upgraded (ws:).
    backgroundScript: webSocketUpgradeListenerBackground,
  },
  {
    description: "Http request in content script is not affected by page csp.",
    version: 2,
    pageCSP: `upgrade-insecure-requests;`,
    data: { url: "http://example.com/plain.html" },
    script: testHttpRequestUpgraded,
    expect: "http:", // we expect the request to not be upgraded.
  },
  {
    description:
      "Http request in content script is not affected by page csp. v3",
    version: 3,
    pageCSP: `upgrade-insecure-requests;`,
    data: { url: "http://example.com/plain.html" },
    script: testHttpRequestUpgraded,
    // TODO bug 1766813: MV3+fetch should use content script CSP.
    expect: "https:", // TODO: we expect the request to not be upgraded (http:).
  },
  {
    description: "content.fetch in content script is affected by page csp.",
    version: 2,
    pageCSP: `upgrade-insecure-requests;`,
    data: { content: true, url: "http://example.com/plain.html" },
    script: testHttpRequestUpgraded,
    expect: "https:", // we expect the request to be upgraded.
  },
];

async function runCSPTest(test) {
  // Set the CSP for the page loaded into the tab.
  gCSP = `${test.pageCSP || gDefaultCSP} report-uri ${CSP_REPORT_PATH}`;
  let data = {
    manifest: {
      manifest_version: test.version || 2,
      content_scripts: [
        {
          matches: ["http://*/plain.html"],
          run_at: "document_idle",
          js: ["content_script.js"],
        },
      ],
      permissions: ["webRequest", "webRequestBlocking"],
      host_permissions: ["<all_urls>"],
      granted_host_permissions: true,
      background: { scripts: ["background.js"] },
    },
    temporarilyInstalled: true,
    files: {
      "content_script.js": `
      (${contentScript})(${JSON.stringify(test.report)}).then(() => {
        browser.test.sendMessage("violationEvent");
      });
      (${test.script})(${JSON.stringify(test.data)}).then(result => {
        if(result !== undefined) { 
          browser.test.sendMessage("result", result);
        }
      });
      `,
      "background.js": `(${test.backgroundScript || (() => {})})()`,
      ...test.files,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();

  let reportPromise = test.report && promiseCSPReport();
  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);

  info(`running: ${test.description}`);
  await extension.awaitMessage("violationEvent");

  let result = await extension.awaitMessage("result");
  equal(result, test.expect, test.description);

  if (test.report) {
    let report = await reportPromise;
    for (let key of Object.keys(test.report)) {
      equal(
        report["csp-report"][key],
        test.report[key],
        `csp-report ${key} matches`
      );
    }
  }

  await extension.unload();
  await contentPage.close();
  clearCache();
}

add_task(async function test_contentscript_csp() {
  for (let test of TESTS) {
    await runCSPTest(test);
  }
});
