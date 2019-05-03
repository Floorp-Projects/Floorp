/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {RemoteAgent} = ChromeUtils.import("chrome://remote/content/RemoteAgent.jsm");
const {RemoteAgentError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

/**
 * Override `add_task` in order to translate chrome-remote-interface exceptions
 * into something that logs better errors on stdout
 */
const original_add_task = add_task.bind(this);
this.add_task = function(test) {
  original_add_task(async function() {
    try {
      await test();
    } catch (e) {
      // Display better error message with the server side stacktrace
      // if an error happened on the server side:
      if (e.response) {
        throw RemoteAgentError.fromJSON(e.response);
      } else {
        throw e;
      }
    }
  });
};

const CRI_URI = "http://example.com/browser/remote/test/browser/chrome-remote-interface.js";

/**
 * Create a test document in an invisible window.
 * This window will be automatically closed on test teardown.
 */
function createTestDocument() {
  const browser = Services.appShell.createWindowlessBrowser(true);
  const webNavigation = browser.docShell.QueryInterface(Ci.nsIWebNavigation);
  // Create a system principal content viewer to ensure there is a valid
  // empty document using system principal and avoid any wrapper issues
  // when using document's JS Objects.
  const system = Services.scriptSecurityManager.getSystemPrincipal();
  webNavigation.createAboutBlankContentViewer(system);

  registerCleanupFunction(() => browser.close());
  return webNavigation.document;
}

/**
 * Retrieve an intance of CDP object from chrome-remote-interface library
 */
async function getCDP() {
  // Instantiate a background test document in order to load the library
  // as in a web page
  const document = createTestDocument();

  // Load chrome-remote-interface.js into this background test document
  const script = document.createElement("script");
  script.setAttribute("src", CRI_URI);
  document.documentElement.appendChild(script);
  await new Promise(resolve => {
    script.addEventListener("load", resolve, {once: true});
  });

  const window = document.defaultView.wrappedJSObject;

  // Implements `criRequest` to be called by chrome-remeote-interface
  // library in order to do the cross-domain http request, which,
  // in a regular Web page, is impossible.
  window.criRequest = (options, callback) => {
    const {host, port, path} = options;
    const url = `http://${host}:${port}${path}`;
    const xhr = new XMLHttpRequest();
    xhr.open("GET", url, true);

    // Prevent "XML Parsing Error: syntax error" error messages
    xhr.overrideMimeType("text/plain");

    xhr.send(null);
    xhr.onload = () => callback(null, xhr.responseText);
    xhr.onerror = e => callback(e, null);
  };

  return window.CDP;
}

function getTargets(CDP) {
  return new Promise((resolve, reject) => {
    CDP.List(null, (err, targets) => {
      if (err) {
        reject(err);
        return;
      }
      resolve(targets);
    });
  });
}
