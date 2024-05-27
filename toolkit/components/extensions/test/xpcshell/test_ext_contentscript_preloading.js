"use strict";

/**
 * Extension content scripts execution happens in two stages:
 * 1. preload content script when a request for a document is observed.
 * 2. actual content script execution when the document has loaded.
 *
 * This is generally an internal implementation detail and an optimization, but
 * if it does not happen, it may lead to difficult-to-diagnose intermittent
 * failures as seen at https://bugzilla.mozilla.org/show_bug.cgi?id=1583700#c12
 * This test hooks the internal content script execution mechanism to confirm
 * that preloading happens as expected.
 *
 * There are some cases where preload is triggered unexpectedly (or not),
 * especially when null principals (e.g. sandboxed frames) are involved,
 * or when principal-inheriting URLs (e.g. about:blank and blob:) are involved.
 * This is not ideal but as long as we don't execute when we should not, we are
 * good. See comment at DocInfo::PrincipalURL in WebExtensionPolicy.cpp
 */

const server = createHttpServer({ hosts: ["example.com"] });
let gRequestCount = 0;
server.registerPathHandler("/dummy", (request, response) => {
  ++gRequestCount;
  response.setStatusLine(request.httpVersion, 200, "OK");
  // The test will add iframes.
  response.write("(dummy, no iframes from server)");
});
server.registerPathHandler("/sandboxed", (request, response) => {
  ++gRequestCount;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Security-Policy", "sandbox allow-scripts;");
  response.write("This page has an opaque origin.");
});

// Ensure that we can detect content script injections for the given extension
// in the process that is currently loaded in |contentPage|.
async function ensureContentScriptDetector(extension, contentPage) {
  await contentPage.spawn([extension.id], extensionId => {
    const { ExtensionProcessScript } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionProcessScript.sys.mjs"
    );
    function log(contentScript, isPreload) {
      const policy = contentScript.extension;
      if (policy.id === extensionId) {
        policy._testOnlySeenContentScriptInjections ??= [];
        policy._testOnlySeenContentScriptInjections.push({
          matches: contentScript.matches.patterns.map(p => p.pattern),
          isPreload,
        });
      }
    }

    // Note: we overwrite the ExtensionProcessScript methods without a way to
    // undo that in this test. The helper is carefully designed to avoid any
    // side effects other than on the specified extension.
    const { preloadContentScript, loadContentScript } = ExtensionProcessScript;
    ExtensionProcessScript.preloadContentScript = contentScript => {
      log(contentScript, true);
      return preloadContentScript(contentScript);
    };
    ExtensionProcessScript.loadContentScript = (contentScript, window) => {
      log(contentScript, false);
      return loadContentScript(contentScript, window);
    };
  });
}

async function getSeenContentScriptInjections(extension, contentPage) {
  return contentPage.spawn([extension.id], extensionId => {
    let policy = this.content.WebExtensionPolicy.getByID(extensionId);
    // Clear the logs and return what we had.
    return policy._testOnlySeenContentScriptInjections?.splice(0);
  });
}

add_task(async function test_preload_at_http_toplevel() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/dummy?toplevelonly"],
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);
  await contentPage.loadURL("http://example.com/dummy?toplevelonly");

  await extension.awaitMessage("script_run");

  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?toplevelonly"], isPreload: true },
      { matches: ["*://example.com/dummy?toplevelonly"], isPreload: false },
    ],
    "Should have observed preload for http toplevel navigation"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_preload_at_http_iframe() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/dummy?in_iframe"],
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?toplevel-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);

  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.src = "http://example.com/dummy?in_iframe";
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");

  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?in_iframe"], isPreload: true },
      { matches: ["*://example.com/dummy?in_iframe"], isPreload: false },
    ],
    "Should have observed preload for http frame"
  );

  await contentPage.close();
  await extension.unload();
});

// Verify that content script preloading and loading works in http:-frame
// inserted by a content script. This is special because a content script has
// an expanded principal.
add_task(async function test_preload_at_http_iframe_from_content_script() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/dummy?do_load_frame"],
          js: ["do_load_frame.js"],
          run_at: "document_end",
        },
        {
          matches: ["*://example.com/dummy?in_frame_from_cs"],
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "do_load_frame.js": () => {
        let f = document.createElement("iframe");
        f.src = "http://example.com/dummy?in_frame_from_cs";
        document.body.append(f);
      },
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);
  await contentPage.loadURL("http://example.com/dummy?do_load_frame");
  await extension.awaitMessage("script_run");

  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?do_load_frame"], isPreload: true },
      { matches: ["*://example.com/dummy?do_load_frame"], isPreload: false },
      { matches: ["*://example.com/dummy?in_frame_from_cs"], isPreload: true },
      { matches: ["*://example.com/dummy?in_frame_from_cs"], isPreload: false },
    ],
    "Should have observed preload for http frame injected by content script"
  );

  await contentPage.close();
  await extension.unload();
});

// Start extension whose content script executes in a page that existed prior
// to extension startup. Should execute immediately without preloading.
add_task(async function test_no_preload_in_existing_documents() {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?existing_doc"
  );
  const extensionId = "@extension-with-content-script-in-existing-doc";
  await ensureContentScriptDetector({ id: extensionId }, contentPage);
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      content_scripts: [
        {
          matches: ["*://example.com/dummy?existing_doc"],
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  await extension.awaitMessage("script_run");

  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [{ matches: ["*://example.com/dummy?existing_doc"], isPreload: false }],
    "Should not preload when scripts are executed as part of extension startup"
  );

  await contentPage.close();
  await extension.unload();
});

// No preload for content scripts at about:blank.
// But preload at about:srcdoc is supported (due to principal inheritance).
add_task(async function test_preload_at_about_blank_iframe() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true, // For "about:*" match pattern sanity check below.
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/dummy?with_blank"],
          match_about_blank: true,
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          matches: ["*://example.com/*"],
          exclude_matches: ["*://example.com/dummy?initial-without_cs"],
          match_origin_as_fallback: true,
          all_frames: true,
          js: ["done2.js"],
          run_at: "document_end",
        },
        {
          // about:blank / about:srcdoc matching is based on the principal URL.
          // To verify that the principal URL is used instead of about:blank at
          // preloading, add a script that should not match.
          // If preloading were to use the incorrect URL, then this script
          // would unexpectedly execute.
          // note: "about:*" match pattern requires isPrivileged:true.
          matches: ["about:*"],
          match_about_blank: true,
          match_origin_as_fallback: true,
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          // The match_origin_as_fallback option should supersede
          // match_about_blank. Despite match_about_blank being set to true,
          // it should be interpreted as false because match_origin_as_fallback
          // was explicitly set to false.
          matches: ["*://example.com/*", "*://3/"], // 3 to distinguish from ^.
          exclude_matches: ["*://example.com/dummy?initial-without_cs"],
          match_about_blank: true,
          match_origin_as_fallback: false,
          all_frames: true,
          js: ["done3.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
      "done2.js": `browser.test.sendMessage("script_run2");`,
      "done3.js": `browser.test.sendMessage("script_run3");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);

  await contentPage.loadURL("http://example.com/dummy?with_blank");
  await extension.awaitMessage("script_run");
  await extension.awaitMessage("script_run2");
  await extension.awaitMessage("script_run3");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?with_blank"], isPreload: true },
      { matches: ["*://example.com/*"], isPreload: true },
      { matches: ["*://example.com/*", "*://3/"], isPreload: true },
      { matches: ["*://example.com/dummy?with_blank"], isPreload: false },
      { matches: ["*://example.com/*"], isPreload: false },
      { matches: ["*://example.com/*", "*://3/"], isPreload: false },
    ],
    "Should have observed preload in initial http document"
  );

  // Setup done, now let's run the actual test!

  info("Testing about:blank frame with match_about_blank");
  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  await extension.awaitMessage("script_run2");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      // We don't preload even though we could. See comment at top of file.
      { matches: ["*://example.com/dummy?with_blank"], isPreload: false },
      { matches: ["*://example.com/*"], isPreload: false },
    ],
    "Preloading is NOT supported in about:blank"
  );

  info("Testing javascript: frame with match_about_blank");
  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.src = "javascript:'javascript:-URL loads in about:blank doc'";
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  await extension.awaitMessage("script_run2");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      // We don't preload even though we could. See comment at top of file.
      { matches: ["*://example.com/dummy?with_blank"], isPreload: false },
      { matches: ["*://example.com/*"], isPreload: false },
    ],
    "Preloading is NOT supported in about:blank (javascript:-URL)"
  );

  info("Testing about:srcdoc frame with match_about_blank");
  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.srcdoc = "This is about:srcdoc";
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  await extension.awaitMessage("script_run2");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?with_blank"], isPreload: true },
      { matches: ["*://example.com/*"], isPreload: true },
      { matches: ["*://example.com/dummy?with_blank"], isPreload: false },
      { matches: ["*://example.com/*"], isPreload: false },
    ],
    "Preloading is supported in about:srcdoc"
  );

  info("Testing sandboxed about:srcdoc frame with match_about_blank");
  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.srcdoc = "This is about:srcdoc in a sandbox";
    f.sandbox = "allow-scripts"; // no allow-same-origin.
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run2");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      // We preload when we should not. See comment at top of file.
      { matches: ["*://example.com/dummy?with_blank"], isPreload: true },
      // With match_origin_as_fallback, we should inject despite the opaque
      // origin of the sandbox:.
      { matches: ["*://example.com/*"], isPreload: true },
      { matches: ["*://example.com/*"], isPreload: false },
    ],
    "sandboxed about:srcdoc requires match_origin_as_fallback:true"
  );

  await contentPage.close();
  await extension.unload();
});

// No preload for content scripts at blob:-URLs.
add_task(async function test_no_preload_at_blob_url_iframe() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          // Note: match_origin_as_fallback is supposed to only work when
          // "matches" has a wildcard path. In our implementation, blob:-URLs
          // have a principal URL that may include a path rather than just the
          // origin, so we can match blob:-URLs created from specific paths.
          // This behavior is NOT documented, but relied upon for convenience
          // here.
          matches: ["*://example.com/dummy?with_blob_url"],
          match_origin_as_fallback: true,
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);

  await contentPage.loadURL("http://example.com/dummy?with_blob_url");
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?with_blob_url"], isPreload: true },
      { matches: ["*://example.com/dummy?with_blob_url"], isPreload: false },
    ],
    "Should have observed preload in initial http document"
  );

  // Setup done, now let's run the actual test!

  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.src = this.content.eval(`URL.createObjectURL(new Blob(["blob:-doc"]))`);
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      // We don't preload even though we could. See comment at top of file.
      { matches: ["*://example.com/dummy?with_blob_url"], isPreload: false },
    ],
    "Preloading is NOT supported in blob:-URLs"
  );
  await contentPage.close();
  await extension.unload();
});

// A sandboxed page has an opaque origin. A http page can be sandboxed by
// the "sandbox" attribute on an iframe.
add_task(async function test_preload_at_http_iframe_with_sandbox_attr() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/dummy?sandbox_parent"],
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          matches: ["*://example.com/dummy?sandbox_iframe"],
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          // Sanity check: should not execute.
          matches: ["<all_urls>"],
          exclude_matches: [
            "*://example.com/dummy?initial-without_cs",
            "*://example.com/dummy?sandbox_parent",
            "*://example.com/dummy?sandbox_iframe",
          ],
          match_about_blank: true,
          match_origin_as_fallback: true,
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);
  await contentPage.loadURL("http://example.com/dummy?sandbox_parent");
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?sandbox_parent"], isPreload: true },
      { matches: ["*://example.com/dummy?sandbox_parent"], isPreload: false },
    ],
    "Should have observed preload in initial http document"
  );

  // Setup done, now let's run the actual test!

  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.src = "http://example.com/dummy?sandbox_iframe";
    f.sandbox = "allow-scripts"; // no allow-same-origin.
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?sandbox_iframe"], isPreload: true },
      { matches: ["*://example.com/dummy?sandbox_iframe"], isPreload: false },
    ],
    "Should not observe any loads of non-matching sandboxed http document"
  );

  await contentPage.close();
  await extension.unload();
});

// A sandboxed page has an opaque origin. A http page can be sandboxed by
// the "sandbox" directive in the response header.
add_task(async function test_preload_at_http_csp_sandbox() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/sandboxed"],
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          // Sanity check: should not execute.
          matches: ["<all_urls>"],
          exclude_matches: [
            "*://example.com/dummy?initial-without_cs",
            "*://example.com/sandboxed",
          ],
          match_about_blank: true,
          match_origin_as_fallback: true,
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);

  await contentPage.loadURL("http://example.com/sandboxed");
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/sandboxed"], isPreload: true },
      { matches: ["*://example.com/sandboxed"], isPreload: false },
    ],
    "Should not observe any loads of non-matching sandboxed http document"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_preload_at_data_url() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/dummy?data_parent"],
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          // Sanity check: should not execute.
          matches: ["<all_urls>", "*://1/"], // 1 to distinguish from below.
          exclude_matches: [
            "*://example.com/dummy?initial-without_cs",
            "*://example.com/dummy?data_parent",
          ],
          match_about_blank: true, // instead of match_origin_as_fallback:true.
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
        {
          matches: ["<all_urls>", "*://2/"], // 2 to distinguish from above.
          exclude_matches: [
            "*://example.com/dummy?initial-without_cs",
            "*://example.com/dummy?data_parent",
          ],
          match_origin_as_fallback: true, // instead of match_about_blank:true.
          all_frames: true,
          js: ["done.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      "done.js": `browser.test.sendMessage("script_run");`,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?initial-without_cs"
  );
  await ensureContentScriptDetector(extension, contentPage);
  await contentPage.loadURL("http://example.com/dummy?data_parent");
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      { matches: ["*://example.com/dummy?data_parent"], isPreload: true },
      { matches: ["*://example.com/dummy?data_parent"], isPreload: false },
    ],
    "Should have observed preload in initial http document"
  );

  // Setup done, now let's run the actual test!

  info("Testing plain data:-URL in iframe");
  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.src = "data:,data_url_in_iframe";
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      // We preload when we should not. See comment at top of file.
      { matches: ["<all_urls>", "*://1/"], isPreload: true },
      // With match_origin_as_fallback:true, this is expected:
      { matches: ["<all_urls>", "*://2/"], isPreload: true },
      { matches: ["<all_urls>", "*://2/"], isPreload: false },
    ],
    "Should match data:-URI when match_origin_as_fallback is true"
  );

  info("Testing sandboxed data:-URL in iframe");
  await contentPage.spawn([], () => {
    let f = this.content.wrappedJSObject.document.createElement("iframe");
    f.src = "data:,data_url_in_sandboxed_iframe";
    f.sandbox = "allow-scripts"; // no allow-same-origin.
    this.content.wrappedJSObject.document.body.append(f);
  });
  await extension.awaitMessage("script_run");
  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    [
      // We preload when we should not. See comment at top of file.
      { matches: ["<all_urls>", "*://1/"], isPreload: true },
      // With match_origin_as_fallback:true, this is expected:
      { matches: ["<all_urls>", "*://2/"], isPreload: true },
      { matches: ["<all_urls>", "*://2/"], isPreload: false },
    ],
    "Should match sandboxed data:-URI when match_origin_as_fallback is true"
  );

  await contentPage.close();
  await extension.unload();
});

// No execution nor preload for content scripts at view-source:-URLs.
add_task(async function test_no_preload_nor_execution_at_view_source() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["*://example.com/*"],
          match_origin_as_fallback: true,
          js: ["done.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "done.js": `browser.test.fail("Unexpected:: " + origin + document.URL);`,
    },
  });
  await extension.startup();

  gRequestCount = 0;

  // Load initial view-source to get the right process.
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "view-source:http://example.com/dummy?initial-viewSource"
  );
  await ensureContentScriptDetector(extension, contentPage);
  // Since we are not expecting content script execution, there is no event to
  // wait for, so we just load another page to maximize the chance of catching
  // an unexpected content script, if any.
  await contentPage.loadURL("view-source:http://example.com/dummy?viewSource");

  // Also test sandboxed origins (regression test for bug 1897759).
  await contentPage.loadURL("view-source:http://example.com/sandboxed?viewS1");
  // Call ensureContentScriptDetector() again to make sure that we can detect
  // content scripts even if sandboxed origins somehow get their own process.
  await ensureContentScriptDetector(extension, contentPage);
  await contentPage.loadURL("view-source:http://example.com/sandboxed?viewS2");

  Assert.equal(gRequestCount, 4, "Got two view-source requests.");
  gRequestCount = 0;

  Assert.deepEqual(
    await getSeenContentScriptInjections(extension, contentPage),
    undefined,
    "Should not have observed any content scripts at view-source:-URLs"
  );

  await contentPage.close();
  await extension.unload();
});
