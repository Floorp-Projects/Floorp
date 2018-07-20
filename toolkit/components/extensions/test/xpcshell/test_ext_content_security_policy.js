"use strict";

const server = createHttpServer({hosts: ["example.com"]});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

/**
 * Tests that content security policies for an add-on are actually applied to *
 * documents that belong to it. This tests both the base policies and add-on
 * specific policies, and ensures that the parsed policies applied to the
 * document's principal match what was specified in the policy string.
 *
 * @param {object} [customCSP]
 */
async function testPolicy(customCSP = null) {
  let baseURL;

  let baseCSP = {
    "object-src": ["blob:", "filesystem:", "https://*", "moz-extension:", "'self'"],
    "script-src": ["'unsafe-eval'", "'unsafe-inline'", "blob:", "filesystem:", "https://*", "moz-extension:", "'self'"],
  };

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
    equal(JSON.stringify(policy[name].sort()),
          JSON.stringify(expected[name].sort()),
          `Expected value for ${name}`);
  }

  function checkCSP(csp, location) {
    let policies = csp["csp-policies"];

    info(`Base policy for ${location}`);

    equal(policies[0]["report-only"], false, "Policy is not report-only");
    checkSource("object-src", policies[0], baseCSP);
    checkSource("script-src", policies[0], baseCSP);

    info(`Add-on policy for ${location}`);

    equal(policies[1]["report-only"], false, "Policy is not report-only");
    checkSource("object-src", policies[1], addonCSP);
    checkSource("script-src", policies[1], addonCSP);
  }


  function background() {
    browser.test.sendMessage("base-url", browser.extension.getURL("").replace(/\/$/, ""));

    browser.test.sendMessage("background-csp", window.getCSP());
  }

  function tabScript() {
    browser.test.sendMessage("tab-csp", window.getCSP());
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,

    files: {
      "tab.html": `<html><head><meta charset="utf-8">
                   <script src="tab.js"></${"script"}></head></html>`,

      "tab.js": tabScript,

      "content.html": `<html><head><meta charset="utf-8"></head></html>`,
    },

    manifest: {
      content_security_policy,

      web_accessible_resources: ["content.html", "tab.html"],
    },
  });


  function frameScript() {
    // eslint-disable-next-line mozilla/balanced-listeners
    addEventListener("DOMWindowCreated", event => {
      let win = event.target.ownerGlobal;
      function getCSP() {
        let {cspJSON} = Cu.getObjectPrincipal(win);
        return win.wrappedJSObject.JSON.parse(cspJSON);
      }
      Cu.exportFunction(getCSP, win, {defineAs: "getCSP"});
    }, true);
  }
  let frameScriptURL = `data:,(${encodeURI(frameScript)}).call(this)`;
  Services.mm.loadFrameScript(frameScriptURL, true);


  info(`Testing CSP for policy: ${content_security_policy}`);

  await extension.startup();

  baseURL = await extension.awaitMessage("base-url");


  let tabPage = await ExtensionTestUtils.loadContentPage(
    `${baseURL}/tab.html`, {extension});

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy");

  let contentCSP = await contentPage.spawn(`${baseURL}/content.html`, async src => {
    let doc = this.content.document;

    let frame = doc.createElement("iframe");
    frame.src = src;
    doc.body.appendChild(frame);

    await new Promise(resolve => {
      frame.onload = resolve;
    });

    return frame.contentWindow.wrappedJSObject.getCSP();
  });


  let backgroundCSP = await extension.awaitMessage("background-csp");
  checkCSP(backgroundCSP, "background page");

  let tabCSP = await extension.awaitMessage("tab-csp");
  checkCSP(tabCSP, "tab page");

  checkCSP(contentCSP, "content frame");

  await contentPage.close();
  await tabPage.close();

  await extension.unload();

  Services.mm.removeDelayedFrameScript(frameScriptURL);
}

add_task(async function testCSP() {
  await testPolicy(null);

  let hash = "'sha256-NjZhMDQ1YjQ1MjEwMmM1OWQ4NDBlYzA5N2Q1OWQ5NDY3ZTEzYTNmMzRmNjQ5NGU1MzlmZmQzMmMxYmIzNWYxOCAgLQo='";

  await testPolicy({
    "object-src": "'self' https://*.example.com",
    "script-src": `'self' https://*.example.com 'unsafe-eval' ${hash}`,
  });

  await testPolicy({
    "object-src": "'none'",
    "script-src": `'self'`,
  });
});
