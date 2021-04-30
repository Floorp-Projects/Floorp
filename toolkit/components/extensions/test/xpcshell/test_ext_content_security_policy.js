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
baseCSP[2] = {
  "object-src": ["blob:", "filesystem:", "moz-extension:", "'self'"],
  "script-src": [
    "'unsafe-eval'",
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
baseCSP[3] = {
  "object-src": ["'self'"],
  "script-src": ["http://localhost:*", "http://127.0.0.1:*", "'self'"],
  "worker-src": ["http://localhost:*", "http://127.0.0.1:*", "'self'"],
};

/**
 * Tests that content security policies for an add-on are actually applied to *
 * documents that belong to it. This tests both the base policies and add-on
 * specific policies, and ensures that the parsed policies applied to the
 * document's principal match what was specified in the policy string.
 *
 * @param {number} [manifest_version]
 * @param {object} [customCSP]
 */
async function testPolicy(manifest_version = 2, customCSP = null) {
  let baseURL;

  let addonCSP = {
    "object-src": ["'self'"],
    "script-src": ["'self'"],
  };

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
      try {
        // eslint-disable-next-line no-undef
        importScripts(`http://127.0.0.1:${port}/worker.js`);
        postMessage({ loaded: true });
      } catch (e) {
        postMessage({ loaded: false });
      }
    };
  }

  if (manifest_version == 3) {
    let extension_pages = content_security_policy;
    content_security_policy = {
      extension_pages,
    };
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

      web_accessible_resources: ["content.html", "tab.html"],
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

  info(`Testing CSP for policy: ${content_security_policy}`);

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
  // TODO BUG 1685627: This test should fail if localhost is not in the csp.
  ok(workerCSP.loaded, "worker loaded");

  await contentPage.close();
  await tabPage.close();

  await extension.unload();

  Services.mm.removeDelayedFrameScript(frameScriptURL);
}

add_task(async function testCSP() {
  await testPolicy(2, null);

  let hash =
    "'sha256-NjZhMDQ1YjQ1MjEwMmM1OWQ4NDBlYzA5N2Q1OWQ5NDY3ZTEzYTNmMzRmNjQ5NGU1MzlmZmQzMmMxYmIzNWYxOCAgLQo='";

  await testPolicy(2, {
    "object-src": "'self' https://*.example.com",
    "script-src": `'self' https://*.example.com 'unsafe-eval' ${hash}`,
  });

  await testPolicy(2, {
    "object-src": "'none'",
    "script-src": `'self'`,
  });

  await testPolicy(3, {
    "object-src": "'self' http://localhost",
    "script-src": `'self' http://localhost:123 ${hash}`,
    "worker-src": `'self' http://127.0.0.1:*`,
  });

  await testPolicy(3, {
    "object-src": "'none'",
    "script-src": `'self'`,
    "worker-src": `'self'`,
  });
});
