/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.importGlobalProperties(["fetch"]);

const { ExtensionData } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm"
);
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

const SEARCH_EXTENSIONS_PATH = "resource://search-extensions";

function getFileURI(resourceURI) {
  let resHandler = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  let filePath = resHandler.resolveURI(Services.io.newURI(resourceURI));
  return Services.io.newURI(filePath);
}

async function getSearchExtensions() {
  // Fetching the root will give us the directory listing which we can parse
  // for each file name
  let list = await fetch(`${SEARCH_EXTENSIONS_PATH}/`).then(req => req.text());
  return list
    .split("\n")
    .slice(2)
    .reduce((acc, line) => {
      let parts = line.split(" ");
      if (parts.length > 2 && !parts[1].endsWith(".json")) {
        // When the directory listing comes from omni jar each engine
        // has a trailing slash (engine/) which we dont get locally, or want.
        acc.push(parts[1].split("/")[0]);
      }
      return acc;
    }, []);
}

add_task(async function test_validate_manifest() {
  let searchExtensions = await getSearchExtensions();
  ok(
    searchExtensions.length > 0,
    `Found ${searchExtensions.length} search extensions`
  );
  for (const xpi of searchExtensions) {
    info(`loading: ${SEARCH_EXTENSIONS_PATH}/${xpi}/`);
    let fileURI = getFileURI(`${SEARCH_EXTENSIONS_PATH}/${xpi}/`);
    let extension = new ExtensionData(fileURI);
    await extension.loadManifest();
    let locales = await extension.promiseLocales();
    for (let locale of locales.keys()) {
      try {
        let manifest = await extension.getLocalizedManifest(locale);
        ok(!!manifest, `parsed manifest ${xpi.leafName} in ${locale}`);
      } catch (e) {
        ok(
          false,
          `FAIL manifest for ${
            xpi.leafName
          } in locale ${locale} failed ${e} :: ${e.stack}`
        );
      }
    }
  }
});
