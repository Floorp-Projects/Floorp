"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

server.registerPathHandler("/worker.js", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/javascript", false);
  response.write("let x = true;");
});

const baseCSP = [];
// Keep in sync with extensions.webextensions.base-content-security-policy
baseCSP[2] = {
  "script-src": [
    "'unsafe-eval'",
    "'wasm-unsafe-eval'",
    "'unsafe-inline'",
    "blob:",
    "filesystem:",
    "http://localhost:*",
    "http://127.0.0.1:*",
    "https://*",
    "moz-extension:",
    "'self'",
  ],
};
// Keep in sync with extensions.webextensions.base-content-security-policy.v3
baseCSP[3] = {
  "script-src": ["'self'", "'wasm-unsafe-eval'"],
};

/**
 * Tests that content security policies for an add-on are actually applied to *
 * documents that belong to it. This tests both the base policies and add-on
 * specific policies, and ensures that the parsed policies applied to the
 * document's principal match what was specified in the policy string.
 *
 * @param {number} [manifest_version]
 * @param {object} [customCSP]
 * @param {object} expects
 * @param {object} expects.workerEvalAllowed
 * @param {object} expects.workerImportScriptsAllowed
 * @param {object} expects.workerWasmAllowed
 */
async function testPolicy({
  manifest_version = 2,
  customCSP = null,
  expects = {},
}) {
  info(
    `Enter tests for extension CSP with ${JSON.stringify({
      manifest_version,
      customCSP,
    })}`
  );

  let baseURL;

  let addonCSP = {
    "script-src": ["'self'"],
  };

  if (manifest_version < 3) {
    addonCSP["script-src"].push("'wasm-unsafe-eval'");
  }

  let content_security_policy = null;

  if (customCSP) {
    for (let key of Object.keys(customCSP)) {
      addonCSP[key] = customCSP[key].split(/\s+/);
    }

    content_security_policy = Object.keys(customCSP)
      .map(key => `${key} ${customCSP[key]}`)
      .join("; ");
  }

  function checkSource(name, policy, expected) {
    // fallback to script-src when comparing worker-src if policy does not include worker-src
    let policySrc =
      name != "worker-src" || policy[name]
        ? policy[name]
        : policy["script-src"];
    equal(
      JSON.stringify(policySrc.sort()),
      JSON.stringify(expected[name].sort()),
      `Expected value for ${name}`
    );
  }

  function checkCSP(csp, location) {
    let policies = csp["csp-policies"];

    info(`Base policy for ${location}`);
    let base = baseCSP[manifest_version];

    equal(policies[0]["report-only"], false, "Policy is not report-only");
    for (let key in base) {
      checkSource(key, policies[0], base);
    }

    info(`Add-on policy for ${location}`);

    equal(policies[1]["report-only"], false, "Policy is not report-only");
    for (let key in addonCSP) {
      checkSource(key, policies[1], addonCSP);
    }
  }

  function background() {
    browser.test.sendMessage(
      "base-url",
      browser.runtime.getURL("").replace(/\/$/, "")
    );

    browser.test.sendMessage("background-csp", window.getCSP());
  }

  function tabScript() {
    browser.test.sendMessage("tab-csp", window.getCSP());

    const worker = new Worker("worker.js");
    worker.onmessage = event => {
      browser.test.sendMessage("worker-csp", event.data);
    };

    worker.postMessage({});
  }

  function testWorker(port) {
    this.onmessage = () => {
      let importScriptsAllowed;
      let evalAllowed;
      let wasmAllowed;

      try {
        eval("let y = true;"); // eslint-disable-line no-eval
        evalAllowed = true;
      } catch (e) {
        evalAllowed = false;
      }

      try {
        new WebAssembly.Module(
          new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
        );
        wasmAllowed = true;
      } catch (e) {
        wasmAllowed = false;
      }

      try {
        // eslint-disable-next-line no-undef
        importScripts(`http://127.0.0.1:${port}/worker.js`);
        importScriptsAllowed = true;
      } catch (e) {
        importScriptsAllowed = false;
      }

      postMessage({ evalAllowed, importScriptsAllowed, wasmAllowed });
    };
  }

  let web_accessible_resources = ["content.html", "tab.html"];
  if (manifest_version == 3) {
    let extension_pages = content_security_policy;
    content_security_policy = {
      extension_pages,
    };
    let resources = web_accessible_resources;
    web_accessible_resources = [
      { resources, matches: ["http://example.com/*"] },
    ];
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,

    files: {
      "tab.html": `<html><head><meta charset="utf-8">
                   <script src="tab.js"></${"script"}></head></html>`,

      "tab.js": tabScript,

      "content.html": `<html><head><meta charset="utf-8"></head></html>`,
      "worker.js": `(${testWorker})(${server.identity.primaryPort})`,
    },

    manifest: {
      manifest_version,
      content_security_policy,
      web_accessible_resources,
    },
  });

  function frameScript() {
    // eslint-disable-next-line mozilla/balanced-listeners
    addEventListener(
      "DOMWindowCreated",
      event => {
        let win = event.target.ownerGlobal;
        function getCSP() {
          let { cspJSON } = win.document;
          return win.wrappedJSObject.JSON.parse(cspJSON);
        }
        Cu.exportFunction(getCSP, win, { defineAs: "getCSP" });
      },
      true
    );
  }
  let frameScriptURL = `data:,(${encodeURI(frameScript)}).call(this)`;
  Services.mm.loadFrameScript(frameScriptURL, true, true);

  info(`Testing CSP for policy: ${JSON.stringify(content_security_policy)}`);

  await extension.startup();

  baseURL = await extension.awaitMessage("base-url");

  let tabPage = await ExtensionTestUtils.loadContentPage(
    `${baseURL}/tab.html`,
    { extension }
  );

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );

  let contentCSP = await contentPage.spawn(
    `${baseURL}/content.html`,
    async src => {
      let doc = this.content.document;

      let frame = doc.createElement("iframe");
      frame.src = src;
      doc.body.appendChild(frame);

      await new Promise(resolve => {
        frame.onload = resolve;
      });

      return frame.contentWindow.wrappedJSObject.getCSP();
    }
  );

  let backgroundCSP = await extension.awaitMessage("background-csp");
  checkCSP(backgroundCSP, "background page");

  let tabCSP = await extension.awaitMessage("tab-csp");
  checkCSP(tabCSP, "tab page");

  checkCSP(contentCSP, "content frame");

  let workerCSP = await extension.awaitMessage("worker-csp");
  equal(
    workerCSP.importScriptsAllowed,
    expects.workerImportAllowed,
    "worker importScript"
  );
  equal(workerCSP.evalAllowed, expects.workerEvalAllowed, "worker eval");
  equal(workerCSP.wasmAllowed, expects.workerWasmAllowed, "worker wasm");

  await contentPage.close();
  await tabPage.close();

  await extension.unload();

  Services.mm.removeDelayedFrameScript(frameScriptURL);
}

add_task(async function testCSP() {
  await testPolicy({
    manifest_version: 2,
    customCSP: null,
    expects: {
      workerEvalAllowed: false,
      workerImportAllowed: false,
      workerWasmAllowed: true,
    },
  });

  let hash =
    "'sha256-NjZhMDQ1YjQ1MjEwMmM1OWQ4NDBlYzA5N2Q1OWQ5NDY3ZTEzYTNmMzRmNjQ5NGU1MzlmZmQzMmMxYmIzNWYxOCAgLQo='";

  await testPolicy({
    manifest_version: 2,
    customCSP: {
      "script-src": `'self' https://*.example.com 'unsafe-eval' ${hash}`,
    },
    expects: {
      workerEvalAllowed: true,
      workerImportAllowed: false,
      workerWasmAllowed: true,
    },
  });

  await testPolicy({
    manifest_version: 2,
    customCSP: {
      "script-src": `'self'`,
    },
    expects: {
      workerEvalAllowed: false,
      workerImportAllowed: false,
      workerWasmAllowed: true,
    },
  });

  await testPolicy({
    manifest_version: 3,
    customCSP: {
      "script-src": `'self' ${hash}`,
      "worker-src": `'self'`,
    },
    expects: {
      workerEvalAllowed: false,
      workerImportAllowed: false,
      workerWasmAllowed: false,
    },
  });

  await testPolicy({
    manifest_version: 3,
    customCSP: {
      "script-src": `'self'`,
      "worker-src": `'self'`,
    },
    expects: {
      workerEvalAllowed: false,
      workerImportAllowed: false,
      workerWasmAllowed: false,
    },
  });

  await testPolicy({
    manifest_version: 3,
    customCSP: {
      "script-src": `'self' 'wasm-unsafe-eval'`,
      "worker-src": `'self' 'wasm-unsafe-eval'`,
    },
    expects: {
      workerEvalAllowed: false,
      workerImportAllowed: false,
      workerWasmAllowed: true,
    },
  });
});
