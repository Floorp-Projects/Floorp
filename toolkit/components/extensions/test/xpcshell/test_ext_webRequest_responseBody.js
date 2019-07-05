"use strict";

/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* eslint-disable no-shadow */

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { ExtensionTestCommon } = ChromeUtils.import(
  "resource://testing-common/ExtensionTestCommon.jsm"
);

const HOSTS = new Set(["example.com"]);

const server = createHttpServer({ hosts: HOSTS });

const BASE_URL = "http://example.com";
const FETCH_ORIGIN = "http://example.com/data/file_sample.html";

const SEQUENTIAL = false;

const PARTS = [
  `<!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <title></title>
    </head>
    <body>`,
  "Lorem ipsum dolor sit amet, <br>",
  "consectetur adipiscing elit, <br>",
  "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. <br>",
  "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. <br>",
  "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. <br>",
  "Excepteur sint occaecat cupidatat non proident, <br>",
  "sunt in culpa qui officia deserunt mollit anim id est laborum.<br>",
  `
    </body>
    </html>`,
].map(part => `${part}\n`);

const TIMEOUT = AppConstants.DEBUG ? 4000 : 800;

function delay(timeout = TIMEOUT) {
  return new Promise(resolve => setTimeout(resolve, timeout));
}

server.registerPathHandler("/slow_response.sjs", async (request, response) => {
  response.processAsync();

  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);

  await delay();

  for (let part of PARTS) {
    try {
      response.write(part);
    } catch (e) {
      // This fails if we attempt to write data after the connection has
      // been closed.
      break;
    }
    await delay();
  }

  response.finish();
});

server.registerPathHandler("/lorem.html.gz", async (request, response) => {
  response.processAsync();

  response.setHeader(
    "Content-Type",
    "Content-Type: text/html; charset=utf-8",
    false
  );
  response.setHeader("Content-Encoding", "gzip", false);

  let data = await OS.File.read(do_get_file("data/lorem.html.gz").path);
  response.write(String.fromCharCode(...new Uint8Array(data)));

  response.finish();
});

server.registerDirectory("/data/", do_get_file("data"));

const TASKS = [
  {
    url: "slow_response.sjs",
    task(filter, resolve, num) {
      let decoder = new TextDecoder("utf-8");

      browser.test.assertEq(
        "uninitialized",
        filter.status,
        `(${num}): Got expected initial status`
      );

      filter.onstart = event => {
        browser.test.assertEq(
          "transferringdata",
          filter.status,
          `(${num}): Got expected onStart status`
        );
      };

      filter.onstop = event => {
        browser.test.fail(
          `(${num}): Got unexpected onStop event while disconnected`
        );
      };

      let n = 0;
      filter.ondata = async event => {
        let str = decoder.decode(event.data, { stream: true });

        if (n < 3) {
          browser.test.assertEq(
            JSON.stringify(PARTS[n]),
            JSON.stringify(str),
            `(${num}): Got expected part`
          );
        }
        n++;

        filter.write(event.data);

        if (n == 3) {
          filter.suspend();

          browser.test.assertEq(
            "suspended",
            filter.status,
            `(${num}): Got expected suspended status`
          );

          let fail = () => {
            browser.test.fail(
              `(${num}): Got unexpected data event while suspended`
            );
          };
          filter.addEventListener("data", fail);

          await delay(TIMEOUT * 3);

          browser.test.assertEq(
            "suspended",
            filter.status,
            `(${num}): Got expected suspended status`
          );

          filter.removeEventListener("data", fail);
          filter.resume();
          browser.test.assertEq(
            "transferringdata",
            filter.status,
            `(${num}): Got expected resumed status`
          );
        } else if (n > 4) {
          filter.disconnect();

          filter.addEventListener("data", () => {
            browser.test.fail(
              `(${num}): Got unexpected data event while disconnected`
            );
          });

          browser.test.assertEq(
            "disconnected",
            filter.status,
            `(${num}): Got expected disconnected status`
          );

          resolve();
        }
      };

      filter.onerror = event => {
        browser.test.fail(
          `(${num}): Got unexpected error event: ${filter.error}`
        );
      };
    },
    verify(response) {
      equal(response, PARTS.join(""), "Got expected final HTML");
    },
  },
  {
    url: "slow_response.sjs",
    task(filter, resolve, num) {
      let decoder = new TextDecoder("utf-8");

      filter.onstop = event => {
        browser.test.fail(
          `(${num}): Got unexpected onStop event while disconnected`
        );
      };

      let n = 0;
      filter.ondata = async event => {
        let str = decoder.decode(event.data, { stream: true });

        if (n < 3) {
          browser.test.assertEq(
            JSON.stringify(PARTS[n]),
            JSON.stringify(str),
            `(${num}): Got expected part`
          );
        }
        n++;

        filter.write(event.data);

        if (n == 3) {
          filter.suspend();

          await delay(TIMEOUT * 3);

          filter.disconnect();

          resolve();
        }
      };

      filter.onerror = event => {
        browser.test.fail(
          `(${num}): Got unexpected error event: ${filter.error}`
        );
      };
    },
    verify(response) {
      equal(response, PARTS.join(""), "Got expected final HTML");
    },
  },
  {
    url: "slow_response.sjs",
    task(filter, resolve, num) {
      let encoder = new TextEncoder("utf-8");

      filter.onstop = event => {
        browser.test.fail(
          `(${num}): Got unexpected onStop event while disconnected`
        );
      };

      let n = 0;
      filter.ondata = async event => {
        n++;

        filter.write(event.data);

        function checkState(state) {
          browser.test.assertEq(
            state,
            filter.status,
            `(${num}): Got expected status`
          );
        }
        if (n == 3) {
          filter.resume();
          checkState("transferringdata");
          filter.suspend();
          checkState("suspended");
          filter.suspend();
          checkState("suspended");
          filter.resume();
          checkState("transferringdata");
          filter.suspend();
          checkState("suspended");

          await delay(TIMEOUT * 3);

          checkState("suspended");
          filter.disconnect();
          checkState("disconnected");

          for (let method of ["suspend", "resume", "close"]) {
            browser.test.assertThrows(
              () => {
                filter[method]();
              },
              /.*/,
              `(${num}): ${method}() should throw while disconnected`
            );
          }

          browser.test.assertThrows(
            () => {
              filter.write(encoder.encode("Foo bar"));
            },
            /.*/,
            `(${num}): write() should throw while disconnected`
          );

          filter.disconnect();

          resolve();
        }
      };

      filter.onerror = event => {
        browser.test.fail(
          `(${num}): Got unexpected error event: ${filter.error}`
        );
      };
    },
    verify(response) {
      equal(response, PARTS.join(""), "Got expected final HTML");
    },
  },
  {
    url: "slow_response.sjs",
    task(filter, resolve, num) {
      let encoder = new TextEncoder("utf-8");
      let decoder = new TextDecoder("utf-8");

      filter.onstop = event => {
        browser.test.fail(`(${num}): Got unexpected onStop event while closed`);
      };

      browser.test.assertThrows(
        () => {
          filter.write(encoder.encode("Foo bar"));
        },
        /.*/,
        `(${num}): write() should throw prior to connection`
      );

      let n = 0;
      filter.ondata = async event => {
        n++;

        filter.write(event.data);

        browser.test.log(
          `(${num}): Got part ${n}: ${JSON.stringify(
            decoder.decode(event.data)
          )}`
        );

        function checkState(state) {
          browser.test.assertEq(
            state,
            filter.status,
            `(${num}): Got expected status`
          );
        }
        if (n == 3) {
          filter.close();

          checkState("closed");

          for (let method of ["suspend", "resume", "disconnect"]) {
            browser.test.assertThrows(
              () => {
                filter[method]();
              },
              /.*/,
              `(${num}): ${method}() should throw while closed`
            );
          }

          browser.test.assertThrows(
            () => {
              filter.write(encoder.encode("Foo bar"));
            },
            /.*/,
            `(${num}): write() should throw while closed`
          );

          filter.close();

          resolve();
        }
      };

      filter.onerror = event => {
        browser.test.fail(
          `(${num}): Got unexpected error event: ${filter.error}`
        );
      };
    },
    verify(response) {
      equal(response, PARTS.slice(0, 3).join(""), "Got expected final HTML");
    },
  },
  {
    url: "lorem.html.gz",
    task(filter, resolve, num) {
      let response = "";
      let decoder = new TextDecoder("utf-8");

      filter.onstart = event => {
        browser.test.log(`(${num}): Request start`);
      };

      filter.onstop = event => {
        browser.test.assertEq(
          "finishedtransferringdata",
          filter.status,
          `(${num}): Got expected onStop status`
        );

        filter.close();
        browser.test.assertEq(
          "closed",
          filter.status,
          `Got expected closed status`
        );

        browser.test.assertEq(
          JSON.stringify(PARTS.join("")),
          JSON.stringify(response),
          `(${num}): Got expected response`
        );

        resolve();
      };

      filter.ondata = event => {
        let str = decoder.decode(event.data, { stream: true });
        response += str;

        filter.write(event.data);
      };

      filter.onerror = event => {
        browser.test.fail(
          `(${num}): Got unexpected error event: ${filter.error}`
        );
      };
    },
    verify(response) {
      equal(response, PARTS.join(""), "Got expected final HTML");
    },
  },
];

function serializeTest(test, num) {
  let url = `${test.url}?test_num=${num}`;
  let task = ExtensionTestCommon.serializeFunction(test.task);

  return `{url: ${JSON.stringify(url)}, task: ${task}}`;
}

add_task(async function() {
  function background(TASKS) {
    async function runTest(test, num, details) {
      browser.test.log(`Running test #${num}: ${details.url}`);

      let filter = browser.webRequest.filterResponseData(details.requestId);

      try {
        await new Promise(resolve => {
          test.task(filter, resolve, num, details);
        });
      } catch (e) {
        browser.test.fail(
          `Task #${num} threw an unexpected exception: ${e} :: ${e.stack}`
        );
      }

      browser.test.log(`Finished test #${num}: ${details.url}`);
      browser.test.sendMessage(`finished-${num}`);
    }

    browser.webRequest.onBeforeRequest.addListener(
      details => {
        for (let [num, test] of TASKS.entries()) {
          if (details.url.endsWith(test.url)) {
            runTest(test, num, details);
            break;
          }
        }
      },
      {
        urls: ["http://example.com/*?test_num=*"],
      },
      ["blocking"]
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `
      const PARTS = ${JSON.stringify(PARTS)};
      const TIMEOUT = ${TIMEOUT};

      ${delay}

      (${background})([${TASKS.map(serializeTest)}])
    `,

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  async function runTest(test, num) {
    let url = `${BASE_URL}/${test.url}?test_num=${num}`;

    let body = await ExtensionTestUtils.fetch(FETCH_ORIGIN, url);

    await extension.awaitMessage(`finished-${num}`);

    info(`Verifying test #${num}: ${url}`);
    await test.verify(body);
  }

  if (SEQUENTIAL) {
    for (let [num, test] of TASKS.entries()) {
      await runTest(test, num);
    }
  } else {
    await Promise.all(TASKS.map(runTest));
  }

  await extension.unload();
});

// Test that registering a listener for a cached response does not cause a crash.
add_task(async function test_cachedResponse() {
  if (AppConstants.platform === "android") {
    return;
  }
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.webRequest.onHeadersReceived.addListener(
        data => {
          let filter = browser.webRequest.filterResponseData(data.requestId);

          filter.onstop = event => {
            filter.close();
          };
          filter.ondata = event => {
            filter.write(event.data);
          };

          if (data.fromCache) {
            browser.test.sendMessage("from-cache");
          }
        },
        {
          urls: ["http://example.com/*/file_sample.html?r=*"],
        },
        ["blocking"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  let url = `${BASE_URL}/data/file_sample.html?r=${Math.random()}`;
  await ExtensionTestUtils.fetch(FETCH_ORIGIN, url);
  await ExtensionTestUtils.fetch(FETCH_ORIGIN, url);
  await extension.awaitMessage("from-cache");

  await extension.unload();
});

// Test that finishing transferring data doesn't overwrite an existing closing/closed state.
add_task(async function test_late_close() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        data => {
          let filter = browser.webRequest.filterResponseData(data.requestId);

          filter.onstop = event => {
            browser.test.fail("Should not receive onstop after close()");
            browser.test.assertEq(
              "closed",
              filter.status,
              "Filter status should still be 'closed'"
            );
            browser.test.assertThrows(() => {
              filter.close();
            });
          };
          filter.ondata = event => {
            filter.write(event.data);
            filter.close();

            browser.test.sendMessage(`done-${data.url}`);
          };
        },
        {
          urls: ["http://example.com/*/file_sample.html?*"],
        },
        ["blocking"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  // This issue involves a race, so several requests in parallel to increase
  // the chances of triggering it.
  let urls = [];
  for (let i = 0; i < 32; i++) {
    urls.push(`${BASE_URL}/data/file_sample.html?r=${Math.random()}`);
  }

  await Promise.all(
    urls.map(url => ExtensionTestUtils.fetch(FETCH_ORIGIN, url))
  );
  await Promise.all(urls.map(url => extension.awaitMessage(`done-${url}`)));

  await extension.unload();
});

add_task(async function test_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.assertEq(
        undefined,
        browser.webRequest.filterResponseData,
        "filterResponseData is undefined without blocking permissions"
      );
    },

    manifest: {
      permissions: ["webRequest", "http://example.com/"],
    },
  });

  await extension.startup();
  await extension.unload();
});

add_task(async function test_invalidId() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      let filter = browser.webRequest.filterResponseData("34159628");

      await new Promise(resolve => {
        filter.onerror = resolve;
      });

      browser.test.assertEq(
        "Invalid request ID",
        filter.error,
        "Got expected error"
      );

      browser.test.notifyPass("invalid-request-id");
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("invalid-request-id");
  await extension.unload();
});
