"use strict";

const server = createHttpServer({ hosts: ["example.com", "example.org"] });
server.registerDirectory("/data/", do_get_file("data"));

let image = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
    "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
);
const IMAGE_ARRAYBUFFER = Uint8Array.from(image, byte => byte.charCodeAt(0))
  .buffer;

async function testImageLoading(src, expectedAction) {
  let imageLoadingPromise = new Promise((resolve, reject) => {
    let cleanupListeners;
    let testImage = document.createElement("img");
    // Set the src via wrappedJSObject so the load is triggered with the
    // content page's principal rather than ours.
    testImage.wrappedJSObject.setAttribute("src", src);

    let loadListener = () => {
      cleanupListeners();
      resolve(expectedAction === "loaded");
    };

    let errorListener = () => {
      cleanupListeners();
      resolve(expectedAction === "blocked");
    };

    cleanupListeners = () => {
      testImage.removeEventListener("load", loadListener);
      testImage.removeEventListener("error", errorListener);
    };

    testImage.addEventListener("load", loadListener);
    testImage.addEventListener("error", errorListener);

    document.body.appendChild(testImage);
  });

  let success = await imageLoadingPromise;
  browser.runtime.sendMessage({
    name: "image-loading",
    expectedAction,
    success,
  });
}

add_task(async function test_web_accessible_resources_csp() {
  function background() {
    browser.runtime.onMessage.addListener((msg, sender) => {
      if (msg.name === "image-loading") {
        browser.test.assertTrue(msg.success, `Image was ${msg.expectedAction}`);
        browser.test.sendMessage(`image-${msg.expectedAction}`);
      } else {
        browser.test.sendMessage(msg);
      }
    });

    browser.test.sendMessage("background-ready");
  }

  function content() {
    window.addEventListener("message", function rcv(event) {
      browser.runtime.sendMessage("script-ran");
      window.removeEventListener("message", rcv);
    });

    testImageLoading(browser.extension.getURL("image.png"), "loaded");

    let testScriptElement = document.createElement("script");
    // Set the src via wrappedJSObject so the load is triggered with the
    // content page's principal rather than ours.
    testScriptElement.wrappedJSObject.setAttribute(
      "src",
      browser.extension.getURL("test_script.js")
    );
    document.head.appendChild(testScriptElement);
    browser.runtime.sendMessage("script-loaded");
  }

  function testScript() {
    window.postMessage("test-script-loaded", "*");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/*/file_csp.html"],
          run_at: "document_end",
          js: ["content_script_helper.js", "content_script.js"],
        },
      ],
      web_accessible_resources: ["image.png", "test_script.js"],
    },
    background,
    files: {
      "content_script_helper.js": `${testImageLoading}`,
      "content_script.js": content,
      "test_script.js": testScript,
      "image.png": IMAGE_ARRAYBUFFER,
    },
  });

  await Promise.all([
    extension.startup(),
    extension.awaitMessage("background-ready"),
  ]);

  let page = await ExtensionTestUtils.loadContentPage(
    `http://example.com/data/file_sample.html`
  );
  await page.spawn(null, () => {
    let { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );
    this.obs = {
      events: [],
      observe(subject, topic, data) {
        this.events.push(subject.QueryInterface(Ci.nsIURI).spec);
      },
      done() {
        Services.obs.removeObserver(this, "csp-on-violate-policy");
        return this.events;
      },
    };
    Services.obs.addObserver(this.obs, "csp-on-violate-policy");
    content.location.href = "http://example.com/data/file_csp.html";
  });

  await Promise.all([
    extension.awaitMessage("image-loaded"),
    extension.awaitMessage("script-loaded"),
    extension.awaitMessage("script-ran"),
  ]);

  let events = await page.spawn(null, () => this.obs.done());
  equal(events.length, 2, "Two items were rejected by CSP");
  for (let url of events) {
    ok(
      url.includes("file_image_bad.png") || url.includes("file_script_bad.js"),
      `Expected file: ${url} rejected by CSP`
    );
  }

  await page.close();
  await extension.unload();
});
