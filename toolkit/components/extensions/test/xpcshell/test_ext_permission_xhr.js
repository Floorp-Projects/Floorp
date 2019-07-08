"use strict";

const server = createHttpServer({
  hosts: ["xpcshell.test", "example.com", "example.org"],
});
server.registerDirectory("/data/", do_get_file("data"));

server.registerPathHandler("/example.txt", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

server.registerPathHandler("/return_headers.sjs", (request, response) => {
  response.setHeader("Content-Type", "text/plain", false);

  let headers = {};
  for (let { data: header } of request.headers) {
    headers[header.toLowerCase()] = request.getHeader(header);
  }

  response.write(JSON.stringify(headers));
});

/* eslint-disable mozilla/balanced-listeners */

add_task(async function test_simple() {
  async function runTests(cx) {
    function xhr(XMLHttpRequest) {
      return url => {
        return new Promise((resolve, reject) => {
          let req = new XMLHttpRequest();
          req.open("GET", url);
          req.addEventListener("load", resolve);
          req.addEventListener("error", reject);
          req.send();
        });
      };
    }

    function run(shouldFail, fetch) {
      function passListener() {
        browser.test.succeed(`${cx}.${fetch.name} pass listener`);
      }

      function failListener() {
        browser.test.fail(`${cx}.${fetch.name} fail listener`);
      }

      /* eslint-disable no-else-return */
      if (shouldFail) {
        return fetch("http://example.org/example.txt").then(
          failListener,
          passListener
        );
      } else {
        return fetch("http://example.com/example.txt").then(
          passListener,
          failListener
        );
      }
      /* eslint-enable no-else-return */
    }

    try {
      await run(true, xhr(XMLHttpRequest));
      await run(false, xhr(XMLHttpRequest));
      await run(true, xhr(window.XMLHttpRequest));
      await run(false, xhr(window.XMLHttpRequest));
      await run(true, fetch);
      await run(false, fetch);
      await run(true, window.fetch);
      await run(false, window.fetch);
    } catch (err) {
      browser.test.fail(`Error: ${err} :: ${err.stack}`);
      browser.test.notifyFail("permission_xhr");
    }
  }

  async function background(runTestsFn) {
    await runTestsFn("bg");
    browser.test.notifyPass("permission_xhr");
  }

  let extensionData = {
    background: `(${background})(${runTests})`,
    manifest: {
      permissions: ["http://example.com/"],
      content_scripts: [
        {
          matches: ["http://xpcshell.test/data/file_permission_xhr.html"],
          js: ["content.js"],
        },
      ],
    },
    files: {
      "content.js": `(${async runTestsFn => {
        await runTestsFn("content");

        window.wrappedJSObject.privilegedFetch = fetch;
        window.wrappedJSObject.privilegedXHR = XMLHttpRequest;

        window.addEventListener("message", function rcv({ data }) {
          switch (data.msg) {
            case "test":
              break;

            case "assertTrue":
              browser.test.assertTrue(data.condition, data.description);
              break;

            case "finish":
              window.removeEventListener("message", rcv);
              browser.test.sendMessage("content-script-finished");
              break;
          }
        });
        window.postMessage("test", "*");
      }})(${runTests})`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://xpcshell.test/data/file_permission_xhr.html"
  );
  await extension.awaitMessage("content-script-finished");
  await contentPage.close();

  await extension.awaitFinish("permission_xhr");
  await extension.unload();
});

// This test case ensures that a WebExtension content script can still use the same
// XMLHttpRequest and fetch APIs that the webpage can use and be recognized from
// the target server with the same origin and referer headers of the target webpage
// (see Bug 1295660 for a rationale).
add_task(async function test_page_xhr() {
  async function contentScript() {
    const content = this.content;

    const { webpageFetchResult, webpageXhrResult } = await new Promise(
      resolve => {
        const listenPageMessage = event => {
          if (!event.data || event.data.type !== "testPageGlobals") {
            return;
          }

          window.removeEventListener("message", listenPageMessage);

          browser.test.assertEq(
            true,
            !!content.XMLHttpRequest,
            "The content script should have access to content.XMLHTTPRequest"
          );
          browser.test.assertEq(
            true,
            !!content.fetch,
            "The content script should have access to window.pageFetch"
          );

          resolve(event.data);
        };

        window.addEventListener("message", listenPageMessage);

        window.postMessage({}, "*");
      }
    );

    const url = new URL("/return_headers.sjs", location).href;

    await Promise.all([
      new Promise((resolve, reject) => {
        const req = new content.XMLHttpRequest();
        req.open("GET", url);
        req.addEventListener("load", () =>
          resolve(JSON.parse(req.responseText))
        );
        req.addEventListener("error", reject);
        req.send();
      }),
      content.fetch(url).then(res => res.json()),
    ])
      .then(async ([xhrResult, fetchResult]) => {
        browser.test.assertEq(
          webpageFetchResult.referer,
          fetchResult.referer,
          "window.pageFetch referrer is the same of a webpage fetch request"
        );
        browser.test.assertEq(
          webpageFetchResult.origin,
          fetchResult.origin,
          "window.pageFetch origin is the same of a webpage fetch request"
        );

        browser.test.assertEq(
          webpageXhrResult.referer,
          xhrResult.referer,
          "content.XMLHttpRequest referrer is the same of a webpage fetch request"
        );
      })
      .catch(error => {
        browser.test.fail(`Unexpected error: ${error}`);
      });

    browser.test.notifyPass("content-script-page-xhr");
  }

  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://xpcshell.test/*"],
          js: ["content.js"],
        },
      ],
    },
    files: {
      "content.js": `(${contentScript})()`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://xpcshell.test/data/file_page_xhr.html"
  );
  await extension.awaitFinish("content-script-page-xhr");
  await contentPage.close();

  await extension.unload();
});
