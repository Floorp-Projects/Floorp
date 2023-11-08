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

  sinon.stub(settings, "get").returns(config);
  await AddonTestUtils.promiseStartupManager();
  await ss.init();
});
