"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

const WIN = `<html><body>dummy page setting a same-site cookie</body></html>`;

// Small red image.
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

server.registerPathHandler("/same_site_cookies", (request, response) => {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString === "loadWin") {
    response.write(WIN);
    return;
  }

  // using startsWith and discard the math random
  if (request.queryString.startsWith("loadImage")) {
    response.setHeader(
      "Set-Cookie",
      "myKey=mySameSiteExtensionCookie; samesite=strict",
      true
    );
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  if (request.queryString === "loadXHR") {
    let cookie = "noCookie";
    if (request.hasHeader("Cookie")) {
      cookie = request.getHeader("Cookie");
    }
    response.setHeader("Content-Type", "text/plain");
    response.write(cookie);
    return;
  }

  // We should never get here, but just in case return something unexpected.
  response.write("D'oh");
});

/* Description of the test:
 * (1) We load an image from mochi.test which sets a same site cookie
 * (2) We have the web extension perform an XHR request to mochi.test
 * (3) We verify the web-extension can access the same-site cookie
 */

add_task(async function test_webRequest_same_site_cookie_access() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://example.com/*"],
      content_scripts: [
        {
          matches: ["http://example.com/*"],
          run_at: "document_end",
          js: ["content_script.js"],
        },
      ],
    },

    background() {
      browser.test.onMessage.addListener(msg => {
        if (msg === "verify-same-site-cookie-moz-extension") {
          let xhr = new XMLHttpRequest();
          try {
            xhr.open(
              "GET",
              "http://example.com/same_site_cookies?loadXHR",
              true
            );
            xhr.onload = function() {
              browser.test.assertEq(
                "myKey=mySameSiteExtensionCookie",
                xhr.responseText,
                "cookie should be accessible from moz-extension context"
              );
              browser.test.sendMessage("same-site-cookie-test-done");
            };
            xhr.onerror = function() {
              browser.test.fail("xhr onerror");
              browser.test.sendMessage("same-site-cookie-test-done");
            };
          } catch (e) {
            browser.test.fail("xhr failure: " + e);
          }
          xhr.send();
        }
      });
    },

    files: {
      "content_script.js": function() {
        let myImage = document.createElement("img");
        // Set the src via wrappedJSObject so the load is triggered with the
        // content page's principal rather than ours.
        myImage.wrappedJSObject.setAttribute(
          "src",
          "http://example.com/same_site_cookies?loadImage" + Math.random()
        );
        myImage.onload = function() {
          browser.test.log("image onload");
          browser.test.sendMessage("image-loaded-and-same-site-cookie-set");
        };
        myImage.onerror = function() {
          browser.test.log("image onerror");
        };
        document.body.appendChild(myImage);
      },
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/same_site_cookies?loadWin"
  );

  await extension.awaitMessage("image-loaded-and-same-site-cookie-set");

  extension.sendMessage("verify-same-site-cookie-moz-extension");
  await extension.awaitMessage("same-site-cookie-test-done");

  await contentPage.close();
  await extension.unload();
});
