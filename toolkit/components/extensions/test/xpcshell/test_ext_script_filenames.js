"use strict";

// There is a rejection emitted when a JS file fails to load. On Android,
// extensions run on the main process and this rejection causes test failures,
// which is essentially why we need to allow the rejection below.
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Unable to load script.*content_script/
);

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

function computeSHA256Hash(text) {
  const hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(Ci.nsICryptoHash.SHA256);
  hasher.update(
    text.split("").map(c => c.charCodeAt(0)),
    text.length
  );
  return hasher.finish(true);
}

// This function represents a dummy content or background script that the test
// cases below should attempt to load but it shouldn't be loaded because we
// check the extensions of JavaScript files in `nsJARChannel`.
function scriptThatShouldNotBeLoaded() {
  browser.test.fail("this should not be executed");
}

function scriptThatAlwaysRuns() {
  browser.test.sendMessage("content-script-loaded");
}

// We use these variables in combination with `scriptThatAlwaysRuns()` to send a
// signal to the extension and avoid the page to be closed too soon.
const alwaysRunsFileName = "always_run.js";
const alwaysRunsContentScript = {
  matches: ["<all_urls>"],
  js: [alwaysRunsFileName],
  run_at: "document_start",
};

add_task(async function test_content_script_filename_without_extension() {
  // Filenames without any extension should not be loaded.
  let invalidFileName = "content_script";
  let extensionData = {
    manifest: {
      content_scripts: [
        alwaysRunsContentScript,
        {
          matches: ["<all_urls>"],
          js: [invalidFileName],
        },
      ],
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [alwaysRunsFileName]: scriptThatAlwaysRuns,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitMessage("content-script-loaded");

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_content_script_filename_with_invalid_extension() {
  let validFileName = "content_script.js";
  let invalidFileName = "content_script.xyz";
  let extensionData = {
    manifest: {
      content_scripts: [
        alwaysRunsContentScript,
        {
          matches: ["<all_urls>"],
          js: [validFileName, invalidFileName],
        },
      ],
    },
    files: {
      // This makes sure that, when one of the content scripts fails to load,
      // none of the content scripts are executed.
      [validFileName]: scriptThatShouldNotBeLoaded,
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [alwaysRunsFileName]: scriptThatAlwaysRuns,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitMessage("content-script-loaded");

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_bg_script_injects_script_with_invalid_ext() {
  function backgroundScript() {
    browser.test.sendMessage("background-script-loaded");
  }

  let validFileName = "background.js";
  let invalidFileName = "invalid_background.xyz";
  let extensionData = {
    background() {
      const script = document.createElement("script");
      script.src = "./invalid_background.xyz";
      document.head.appendChild(script);

      const validScript = document.createElement("script");
      validScript.src = "./background.js";
      document.head.appendChild(validScript);
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [validFileName]: backgroundScript,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitMessage("background-script-loaded");

  await extension.unload();
});

add_task(async function test_background_scripts() {
  function backgroundScript() {
    browser.test.sendMessage("background-script-loaded");
  }

  let validFileName = "background.js";
  let invalidFileName = "invalid_background.xyz";
  let extensionData = {
    manifest: {
      background: {
        scripts: [invalidFileName, validFileName],
      },
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [validFileName]: backgroundScript,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitMessage("background-script-loaded");

  await extension.unload();
});

add_task(async function test_background_page_injects_scripts_inline() {
  function injectedBackgroundScript() {
    browser.test.log(
      "inline script injectedBackgroundScript has been executed"
    );
    browser.test.sendMessage("background-script-loaded");
  }

  let backgroundHtmlPage = "background_page.html";
  let validFileName = "injected_background.js";
  let invalidFileName = "invalid_background.xyz";

  let inlineScript = `(${function() {
    const script = document.createElement("script");
    script.src = "./invalid_background.xyz";
    document.head.appendChild(script);
    const validScript = document.createElement("script");
    validScript.src = "./injected_background.js";
    document.head.appendChild(validScript);
  }})()`;

  const inlineScriptSHA256 = computeSHA256Hash(inlineScript);

  info(
    `Computed sha256 for the inline script injectedBackgroundScript: ${inlineScriptSHA256}`
  );

  let extensionData = {
    manifest: {
      background: { page: backgroundHtmlPage },
      content_security_policy: [
        "script-src",
        "'self'",
        `'sha256-${inlineScriptSHA256}'`,
        ";",
        "object-src",
        "'self'",
        ";",
      ].join(" "),
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [validFileName]: injectedBackgroundScript,
      "pre-script.js": () => {
        window.onsecuritypolicyviolation = evt => {
          const { violatedDirective, originalPolicy } = evt;
          browser.test.fail(
            `Unexpected csp violation: ${JSON.stringify({
              violatedDirective,
              originalPolicy,
            })}`
          );
          // Let the test to fail immediately when an unexpected csp violation
          // prevented the inline script from being executed successfully.
          browser.test.sendMessage("background-script-loaded");
        };
      },
      [backgroundHtmlPage]: `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8"></head>
            <script src="pre-script.js"></script>
            <script>${inlineScript}</script>
          </head>
        </html>`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitMessage("background-script-loaded");

  await extension.unload();
});

add_task(async function test_background_page_injects_scripts() {
  // This is the initial background script loaded in the HTML page.
  function backgroundScript() {
    const script = document.createElement("script");
    script.src = "./invalid_background.xyz";
    document.head.appendChild(script);

    const validScript = document.createElement("script");
    validScript.src = "./injected_background.js";
    document.head.appendChild(validScript);
  }

  // This is the script injected by the script defined in `backgroundScript()`.
  function injectedBackgroundScript() {
    browser.test.sendMessage("background-script-loaded");
  }

  let backgroundHtmlPage = "background_page.html";
  let validFileName = "injected_background.js";
  let invalidFileName = "invalid_background.xyz";
  let extensionData = {
    manifest: {
      background: { page: backgroundHtmlPage },
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [validFileName]: injectedBackgroundScript,
      [backgroundHtmlPage]: `
        <html>
          <head>
            <meta charset="utf-8"></head>
            <script src="./background.js"></script>
          </head>
        </html>`,
      "background.js": backgroundScript,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitMessage("background-script-loaded");

  await extension.unload();
});

add_task(async function test_background_script_registers_content_script() {
  let invalidFileName = "content_script";
  let extensionData = {
    manifest: {
      permissions: ["<all_urls>"],
    },
    async background() {
      await browser.contentScripts.register({
        js: [{ file: "/content_script" }],
        matches: ["<all_urls>"],
      });
      browser.test.sendMessage("background-script-loaded");
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitMessage("background-script-loaded");

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_web_accessible_resources() {
  function contentScript() {
    const script = document.createElement("script");
    script.src = browser.runtime.getURL("content_script.css");
    script.onerror = () => {
      browser.test.sendMessage("second-content-script-loaded");
    };

    document.head.appendChild(script);
  }

  let contentScriptFileName = "content_script.js";
  let invalidFileName = "content_script.css";
  let extensionData = {
    manifest: {
      web_accessible_resources: [invalidFileName],
      content_scripts: [
        alwaysRunsContentScript,
        {
          matches: ["<all_urls>"],
          js: [contentScriptFileName],
        },
      ],
    },
    files: {
      [invalidFileName]: scriptThatShouldNotBeLoaded,
      [contentScriptFileName]: contentScript,
      [alwaysRunsFileName]: scriptThatAlwaysRuns,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitMessage("content-script-loaded");
  await extension.awaitMessage("second-content-script-loaded");

  await contentPage.close();
  await extension.unload();
});
