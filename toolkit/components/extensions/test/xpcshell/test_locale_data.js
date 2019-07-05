"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const { ExtensionData } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm"
);

async function generateAddon(data) {
  let xpi = AddonTestUtils.createTempWebExtensionFile(data);

  let fileURI = Services.io.newFileURI(xpi);
  let jarURI = NetUtil.newURI(`jar:${fileURI.spec}!/`);

  let extension = new ExtensionData(jarURI);
  await extension.loadManifest();

  return extension;
}

add_task(async function testMissingDefaultLocale() {
  let extension = await generateAddon({
    files: {
      "_locales/en_US/messages.json": {},
    },
  });

  equal(extension.errors.length, 0, "No errors reported");

  await extension.initAllLocales();

  equal(extension.errors.length, 1, "One error reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(
    extension.errors[0].includes('"default_locale" property is required'),
    "Got missing default_locale error"
  );
});

add_task(async function testInvalidDefaultLocale() {
  let extension = await generateAddon({
    manifest: {
      default_locale: "en",
    },

    files: {
      "_locales/en_US/messages.json": {},
    },
  });

  equal(extension.errors.length, 1, "One error reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(
    extension.errors[0].includes(
      "Loading locale file _locales/en/messages.json"
    ),
    "Got invalid default_locale error"
  );

  await extension.initAllLocales();

  equal(extension.errors.length, 2, "Two errors reported");

  info(`Got error: ${extension.errors[1]}`);

  ok(
    extension.errors[1].includes('"default_locale" property must correspond'),
    "Got invalid default_locale error"
  );
});

add_task(async function testUnexpectedDefaultLocale() {
  let extension = await generateAddon({
    manifest: {
      default_locale: "en_US",
    },
  });

  equal(extension.errors.length, 1, "One error reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(
    extension.errors[0].includes(
      "Loading locale file _locales/en-US/messages.json"
    ),
    "Got invalid default_locale error"
  );

  await extension.initAllLocales();

  equal(extension.errors.length, 2, "One error reported");

  info(`Got error: ${extension.errors[1]}`);

  ok(
    extension.errors[1].includes('"default_locale" property must correspond'),
    "Got unexpected default_locale error"
  );
});

add_task(async function testInvalidSyntax() {
  let extension = await generateAddon({
    manifest: {
      default_locale: "en_US",
    },

    files: {
      "_locales/en_US/messages.json":
        '{foo: {message: "bar", description: "baz"}}',
    },
  });

  equal(extension.errors.length, 1, "No errors reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(
    extension.errors[0].includes(
      "Loading locale file _locales/en_US/messages.json: SyntaxError"
    ),
    "Got syntax error"
  );

  await extension.initAllLocales();

  equal(extension.errors.length, 2, "One error reported");

  info(`Got error: ${extension.errors[1]}`);

  ok(
    extension.errors[1].includes(
      "Loading locale file _locales/en_US/messages.json: SyntaxError"
    ),
    "Got syntax error"
  );
});

add_task(async function testExtractLocalizedManifest() {
  let extension = await generateAddon({
    manifest: {
      name: "__MSG_extensionName__",
      default_locale: "en_US",
      icons: {
        "16": "__MSG_extensionIcon__",
      },
    },

    files: {
      "_locales/en_US/messages.json": `{
        "extensionName": {"message": "foo"},
        "extensionIcon": {"message": "icon-en.png"}
      }`,
      "_locales/de_DE/messages.json": `{
        "extensionName": {"message": "bar"},
        "extensionIcon": {"message": "icon-de.png"}
      }`,
    },
  });

  await extension.loadManifest();
  equal(extension.manifest.name, "foo", "name localized");
  equal(extension.manifest.icons["16"], "icon-en.png", "icons localized");

  let manifest = await extension.getLocalizedManifest("de-DE");
  ok(extension.localeData.has("de-DE"), "has de_DE locale");
  equal(manifest.name, "bar", "name localized");
  equal(manifest.icons["16"], "icon-de.png", "icons localized");

  await Assert.rejects(
    extension.getLocalizedManifest("xx-XX"),
    /does not contain the locale xx-XX/,
    "xx-XX does not exist"
  );
});

add_task(async function testRestartThenExtractLocalizedManifest() {
  await AddonTestUtils.promiseStartupManager();

  let wrapper = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "__MSG_extensionName__",
      default_locale: "en_US",
    },
    useAddonManager: "permanent",
    files: {
      "_locales/en_US/messages.json": '{"extensionName": {"message": "foo"}}',
      "_locales/de_DE/messages.json": '{"extensionName": {"message": "bar"}}',
    },
  });

  await wrapper.startup();

  await AddonTestUtils.promiseRestartManager();
  await wrapper.startupPromise;

  let { extension } = wrapper;
  let manifest = await extension.getLocalizedManifest("de-DE");
  ok(extension.localeData.has("de-DE"), "has de_DE locale");
  equal(manifest.name, "bar", "name localized");

  await Assert.rejects(
    extension.getLocalizedManifest("xx-XX"),
    /does not contain the locale xx-XX/,
    "xx-XX does not exist"
  );

  await wrapper.unload();
  await AddonTestUtils.promiseShutdownManager();
});
