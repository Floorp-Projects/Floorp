/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure all the engines defined in the configuration are valid by
// creating a refined configuration that includes all the engines everywhere.

"use strict";

const { SearchService } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchService.sys.mjs"
);

const ss = new SearchService();

add_task(async function test_validate_engines() {
  let settings = RemoteSettings(SearchUtils.SETTINGS_KEY);
  let config = await settings.get();

  if (SearchUtils.newSearchConfigEnabled) {
    // We do not load engines with the same name. However, in search-config-v2
    // we have multiple engines named eBay, so we error out.
    // We never deploy more than one eBay in each environment so this issue
    // won't be a problem.
    // Ignore the error and test the configs can be created to engine objects.
    consoleAllowList.push("Could not load engine");
    config = config.map(obj => {
      if (obj.recordType == "engine") {
        return {
          recordType: "engine",
          identifier: obj.identifier,
          base: {
            name: obj.base.name,
            urls: {
              search: {
                base: obj.base.urls.search.base || "",
                searchTermParamName: "q",
              },
            },
          },
          variants: [
            {
              environment: { allRegionsAndLocales: true },
            },
          ],
        };
      }

      return obj;
    });
  } else {
    config = config.map(e => {
      return {
        appliesTo: [
          {
            included: {
              everywhere: true,
            },
          },
        ],
        webExtension: e.webExtension,
      };
    });
  }

  sinon.stub(settings, "get").returns(config);
  await AddonTestUtils.promiseStartupManager();
  await ss.init();
});
