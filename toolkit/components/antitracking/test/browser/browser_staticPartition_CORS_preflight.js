/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_PAGE =
  "http://example.org/browser/toolkit/components/antitracking/test/browser/empty.html";
const TEST_ANOTHER_PAGE =
  "http://example.com/browser/toolkit/components/antitracking/test/browser/empty.html";
const TEST_PREFLIGHT_IFRAME_PAGE =
  "http://mochi.test:8888/browser/toolkit/components/antitracking/test/browser/empty.html";
const TEST_PREFLIGHT_PAGE =
  "http://example.net/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_CORS_preflight.sjs";

add_task(async function () {
  let uuidGenerator = Services.uuid;

  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      // First, create one tab under one first party.
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_PAGE
      );
      let token = uuidGenerator.generateUUID().toString();

      // Use fetch to verify that preflight cache is working. The preflight
      // cache is keyed by the loading principal and the url. So, we load an
      // iframe with one origin and use fetch in there to ensure we will have
      // the same loading principal.
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [TEST_PREFLIGHT_PAGE, TEST_PREFLIGHT_IFRAME_PAGE, token],
        async (url, iframe_url, token) => {
          let iframe = content.document.createElement("iframe");

          await new Promise(resolve => {
            iframe.onload = () => {
              resolve();
            };
            content.document.body.appendChild(iframe);
            iframe.src = iframe_url;
          });

          await SpecialPowers.spawn(
            iframe,
            [url, token],
            async (url, token) => {
              const test_url = `${url}?token=${token}`;
              let response = await content.fetch(
                new content.Request(test_url, {
                  mode: "cors",
                  method: "GET",
                  headers: [["x-test-header", "check"]],
                })
              );

              is(
                await response.text(),
                "1",
                "The preflight should be sent at first time"
              );

              response = await content.fetch(
                new content.Request(test_url, {
                  mode: "cors",
                  method: "GET",
                  headers: [["x-test-header", "check"]],
                })
              );

              is(
                await response.text(),
                "0",
                "The preflight shouldn't be sent due to the preflight cache"
              );
            }
          );
        }
      );

      // Load the tab with a different first party. And use fetch to check if
      // the preflight cache is partitioned. The fetch will also be performed in
      // the iframe with the same origin as above to ensure we use the same
      // loading principal.
      BrowserTestUtils.startLoadingURIString(
        tab.linkedBrowser,
        TEST_ANOTHER_PAGE
      );
      await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [
          TEST_PREFLIGHT_PAGE,
          TEST_PREFLIGHT_IFRAME_PAGE,
          token,
          networkIsolation,
        ],
        async (url, iframe_url, token, partitioned) => {
          let iframe = content.document.createElement("iframe");

          await new Promise(resolve => {
            iframe.onload = () => {
              resolve();
            };
            content.document.body.appendChild(iframe);
            iframe.src = iframe_url;
          });

          await SpecialPowers.spawn(
            iframe,
            [url, token, partitioned],
            async (url, token, partitioned) => {
              const test_url = `${url}?token=${token}`;

              let response = await content.fetch(
                new content.Request(test_url, {
                  mode: "cors",
                  method: "GET",
                  headers: [["x-test-header", "check"]],
                })
              );

              if (partitioned) {
                is(
                  await response.text(),
                  "1",
                  "The preflight cache should be partitioned"
                );
              } else {
                is(
                  await response.text(),
                  "0",
                  "The preflight cache shouldn't be partitioned"
                );
              }
            }
          );
        }
      );

      BrowserTestUtils.removeTab(tab);
    }
  }
});
