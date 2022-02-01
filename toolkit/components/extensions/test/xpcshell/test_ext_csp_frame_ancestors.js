/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const server = createHttpServer({ hosts: ["example.com", "example.net"] });
server.registerPathHandler("/parent.html", (request, response) => {
  let frameUrl = new URLSearchParams(request.queryString).get("iframe_src");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write(`<!DOCTYPE html><iframe src="${frameUrl}"></iframe>`);
});

// Loads an extension frame as a frame at ancestorOrigins[0], which in turn is
// a child of ancestorOrigins[1], etc.
// The frame should either load successfully, or trigger exactly one failure due
// to one of the ancestorOrigins being blocked by the content_security_policy.
async function checkExtensionLoadInFrame({
  ancestorOrigins,
  content_security_policy,
  expectLoad,
}) {
  const extensionData = {
    manifest: {
      content_security_policy,
      web_accessible_resources: ["parent.html", "frame.html"],
    },
    files: {
      "frame.html": `<!DOCTYPE html><script src="frame.js"></script>`,
      "frame.js": () => {
        browser.test.sendMessage("frame_load_completed");
      },
      "parent.html": `<!DOCTYPE html><body><script src="parent.js"></script>`,
      "parent.js": () => {
        let iframe = document.createElement("iframe");
        iframe.src = new URLSearchParams(location.search).get("iframe_src");
        document.body.append(iframe);
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  const EXTENSION_FRAME_URL = `moz-extension://${extension.uuid}/frame.html`;

  // ancestorOrigins is a list of origins, from the parent up to the top frame.
  let topUrl = EXTENSION_FRAME_URL;
  for (let origin of ancestorOrigins) {
    if (origin === "EXTENSION_ORIGIN") {
      origin = `moz-extension://${extension.uuid}`;
    }
    // origin is either the origin for |server| or the test extension. Both
    // endpoints serve a page at parent.html that embeds iframe_src.
    topUrl = `${origin}/parent.html?iframe_src=${encodeURIComponent(topUrl)}`;
  }

  let cspViolationObserver;
  let cspViolationCount = 0;
  let frameLoadedCount = 0;
  let frameLoadOrFailedPromise = new Promise(resolve => {
    extension.onMessage("frame_load_completed", () => {
      ++frameLoadedCount;
      resolve();
    });
    cspViolationObserver = {
      observe(subject, topic, data) {
        ++cspViolationCount;
        Assert.equal(data, "frame-ancestors", "CSP violation directive");
        resolve();
      },
    };
    Services.obs.addObserver(cspViolationObserver, "csp-on-violate-policy");
  });

  const contentPage = await ExtensionTestUtils.loadContentPage(topUrl);

  // Firstly, wait for the frame load to either complete or fail.
  await frameLoadOrFailedPromise;

  // Secondly, do a round trip to the content process to make sure that any
  // unexpected extra load/failures are observed. This is necessary, because
  // the "csp-on-violate-policy" notification is triggered from the parent,
  // while it may be possible for the load to continue in the child anyway.
  //
  // And while we are at it, this verifies that the CSP does not block regular
  // reads of a file that's part of web_accessible_resources. For comparable
  // results, the load should ideally happen in the parent of the extension
  // frame, but contentPage.fetch only works in the top frame, so this does not
  // work perfectly in case ancestorOrigins.length > 1.
  // But that is OK, as we mainly care about unexpected frame loads/failures.
  equal(
    await contentPage.fetch(EXTENSION_FRAME_URL),
    extensionData.files["frame.html"],
    "web-accessible extension resource can still be read with fetch"
  );

  // Finally, clean up.
  Services.obs.removeObserver(cspViolationObserver, "csp-on-violate-policy");
  await contentPage.close();
  await extension.unload();

  if (expectLoad) {
    equal(cspViolationCount, 0, "Expected no CSP violations");
    equal(
      frameLoadedCount,
      1,
      `Frame should accept ancestors (${ancestorOrigins}) in CSP: ${content_security_policy}`
    );
  } else {
    equal(cspViolationCount, 1, "Expected CSP violation count");
    equal(
      frameLoadedCount,
      0,
      `Frame should reject one of the ancestors (${ancestorOrigins}) in CSP: ${content_security_policy}`
    );
  }
}

add_task(async function test_frame_ancestors_missing_allows_self() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["EXTENSION_ORIGIN"],
    content_security_policy: "default-src 'self'", // missing frame-ancestors.
    expectLoad: true, // an extension can embed itself by default.
  });
});

add_task(async function test_frame_ancestors_self_allows_self() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["EXTENSION_ORIGIN"],
    content_security_policy: "default-src 'self'; frame-ancestors 'self'",
    expectLoad: true,
  });
});

add_task(async function test_frame_ancestors_none_blocks_self() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["EXTENSION_ORIGIN"],
    content_security_policy: "default-src 'self'; frame-ancestors",
    expectLoad: false, // frame-ancestors 'none' blocks extension frame.
  });
});

add_task(async function test_frame_ancestors_missing_allowed_in_web_page() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com"],
    content_security_policy: "default-src 'self'", // missing frame-ancestors
    expectLoad: true, // Web page can embed web-accessible extension frames.
  });
});

add_task(async function test_frame_ancestors_self_blocked_in_web_page() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com"],
    content_security_policy: "default-src 'self'; frame-ancestors 'self'",
    expectLoad: false,
  });
});

add_task(async function test_frame_ancestors_scheme_allowed_in_web_page() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com"],
    content_security_policy: "default-src 'self'; frame-ancestors http:",
    expectLoad: true,
  });
});

add_task(async function test_frame_ancestors_origin_allowed_in_web_page() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com"],
    content_security_policy:
      "default-src 'self'; frame-ancestors http://example.com",
    expectLoad: true,
  });
});

add_task(async function test_frame_ancestors_mismatch_blocked_in_web_page() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com"],
    content_security_policy:
      "default-src 'self'; frame-ancestors http://not.example.com",
    expectLoad: false,
  });
});

add_task(async function test_frame_ancestors_top_mismatch_blocked() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com", "http://example.net"],
    content_security_policy:
      "default-src 'self'; frame-ancestors http://example.com",
    // example.com is allowed, but the top origin (example.net) is rejected.
    expectLoad: false,
  });
});

add_task(async function test_frame_ancestors_parent_mismatch_blocked() {
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.net", "http://example.com"],
    content_security_policy:
      "default-src 'self'; frame-ancestors http://example.com",
    // example.com is allowed, but the parent origin (example.net) is rejected.
    expectLoad: false,
  });
});

add_task(async function test_frame_ancestors_middle_rejected() {
  if (!WebExtensionPolicy.useRemoteWebExtensions) {
    // This test load http://example.com in an extension page, which fails if
    // extensions run in the parent process. This is not a default config on
    // desktop, but see https://bugzilla.mozilla.org/show_bug.cgi?id=1724099
    info("Web pages cannot be loaded in extension page without OOP extensions");
    return;
  }
  await checkExtensionLoadInFrame({
    ancestorOrigins: ["http://example.com", "EXTENSION_ORIGIN"],
    content_security_policy:
      "default-src 'self'; frame-src http: 'self'; frame-ancestors 'self'",
    // Although the top frame has the same origin as the extension, the load
    // should be rejected anyway because there is a non-allowlisted origin in
    // the middle (child of top frame, parent of extension frame).
    expectLoad: false,
  });
});
