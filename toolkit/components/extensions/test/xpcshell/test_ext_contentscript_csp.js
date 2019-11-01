/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

// Enable and turn off report-only so we can validate the results.
Services.prefs.setBoolPref("extensions.content_script_csp.enabled", true);
Services.prefs.setBoolPref("extensions.content_script_csp.report_only", false);

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
const CSP_REPORT = `report-uri http://csplog.example.net${CSP_REPORT_PATH};`;

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

const gDefaultContentScriptCSP =
  "default-src 'self' 'report-sample'; object-src 'self'; script-src 'self';";

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
  {
    description:
      "Image from content script using extension csp. Image is not allowed.",
    pageCSP: `${gDefaultCSP} img-src 'self';`,
    scriptCSP: `${gDefaultContentScriptCSP} img-src 'none';`,
    script: testImage,
    data: { image_url: `${BASE_URL}/data/file_image_good.png` },
    expect: false,
    report: {
      "blocked-uri": `${BASE_URL}/data/file_image_good.png`,
      "document-uri": "moz-extension",
      "violated-directive": "img-src",
    },
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
    description: "Fetch url in content script uses extension csp.",
    pageCSP: `${gDefaultCSP} connect-src 'none';`,
    script: testFetch,
    scriptCSP: `${gDefaultContentScriptCSP} connect-src 'none';`,
    data: { url: `${BASE_URL}/data/file_image_good.png` },
    expect: false,
    report: {
      "blocked-uri": `${BASE_URL}/data/file_image_good.png`,
      "document-uri": "moz-extension",
      "violated-directive": "connect-src",
    },
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
  {
    description: "Fetch url from content script uses page csp.",
    pageCSP: `${gDefaultCSP} connect-src *;`,
    script: testFetch,
    scriptCSP: `${gDefaultContentScriptCSP} connect-src 'none' 'report-sample';`,
    data: {
      content: true,
      url: `${BASE_URL}/data/file_image_good.png`,
    },
    expect: true,
  },

  // TODO Bug 1587939: Eval tests.
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
    scriptCSP: `object-src 'self'; script-src 'self' 'unsafe-eval';`,
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
    description: "Eval in content script uses extension csp.",
    pageCSP: `script-src 'self' 'unsafe-eval';`,
    script: testEval,
    expect: false,
  },
  {
    description: "Eval in content script uses extension csp.  unsafe-eval",
    pageCSP: `default-src 'self'; script-src 'self';`,
    scriptCSP: `object-src 'self'; script-src 'self' 'unsafe-eval';`,
    script: testEval,
    expect: true,
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
    scriptCSP: `object-src 'self'; script-src 'self' 'unsafe-eval';`,
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
    description: "Function in content script uses default extension csp.",
    pageCSP: `default-src 'self'; script-src 'self' 'unsafe-eval';`,
    script: testFunction,
    expect: 0,
  },
  {
    description:
      "Function in content script uses extension csp, with unsafe-eval",
    pageCSP: `default-src 'self'; script-src 'self';`,
    scriptCSP: `default-src 'self'; object-src 'self'; script-src 'self' 'unsafe-eval';`,
    script: testFunction,
    expect: 2,
  },
];

async function runCSPTest(test) {
  // Set the CSP for the page loaded into the tab.
  gCSP = `${test.pageCSP || gDefaultCSP} report-uri ${CSP_REPORT_PATH}`;
  let data = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/plain.html"],
          run_at: "document_idle",
          js: ["content_script.js"],
        },
      ],
      permissions: ["<all_urls>"],
    },

    files: {
      "content_script.js": `
      (${contentScript})(${JSON.stringify(test.report)}).then(() => {
        browser.test.sendMessage("violationEvent");
      });
      (${test.script})(${JSON.stringify(test.data)}).then(result => {
        browser.test.sendMessage("result", result);
      });
      `,
    },
  };
  if (test.scriptCSP) {
    data.manifest.content_security_policy = {
      content_scripts: `${test.scriptCSP} ${CSP_REPORT}`,
    };
  }
  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();

  let reportPromise = test.report && promiseCSPReport();
  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);

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
