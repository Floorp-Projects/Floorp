/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Schemas.jsm");

const {
  validateThemeManifest,
} = Cu.import("resource://gre/modules/Extension.jsm", {});

const BASE_SCHEMA_URL = "chrome://extensions/content/schemas/manifest.json";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";

const baseManifestProperties = [
  "manifest_version",
  "minimum_chrome_version",
  "applications",
  "browser_specific_settings",
  "name",
  "short_name",
  "description",
  "author",
  "version",
  "homepage_url",
  "icons",
  "incognito",
  "background",
  "options_ui",
  "content_scripts",
  "permissions",
  "web_accessible_resources",
  "developer",
];

async function getAdditionalInvalidManifestProperties() {
  let invalidProperties = [];
  await Schemas.load(BASE_SCHEMA_URL);
  for (let [name, url] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCHEMAS)) {
    if (name !== "theme") {
      await Schemas.load(url);
      let types = Schemas.schemaJSON.get(url).deserialize({})[0].types;
      types.forEach(type => {
        if (type.$extend == "WebExtensionManifest") {
          let properties = Object.getOwnPropertyNames(type.properties);
          invalidProperties.push(...properties);
        }
      });
    }
  }

  // Also test an unrecognized property.
  invalidProperties.push("unrecognized_property");

  return invalidProperties;
}

function checkProperties(actual, expected) {
  Assert.equal(actual.length, expected.length, `Should have found ${expected.length} invalid properties`);
  for (let i = 0; i < expected.length; i++) {
    Assert.ok(actual.includes(expected[i]), `The invalid properties should contain "${expected[i]}"`);
  }
}

add_task(async function test_theme_supported_properties() {
  let additionalInvalidProperties = await getAdditionalInvalidManifestProperties();
  let actual = validateThemeManifest([...baseManifestProperties, ...additionalInvalidProperties]);
  let expected = ["background", "permissions", "content_scripts", ...additionalInvalidProperties];
  checkProperties(actual, expected);
});

