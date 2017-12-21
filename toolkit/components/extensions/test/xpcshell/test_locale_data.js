"use strict";

Cu.import("resource://gre/modules/Extension.jsm");

/* globals ExtensionData */

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

async function generateAddon(data) {
  let id = uuidGenerator.generateUUID().number;

  data = Object.assign({embedded: true}, data);
  data.manifest = Object.assign({applications: {gecko: {id}}}, data.manifest);

  let xpi = Extension.generateXPI(data);
  registerCleanupFunction(() => {
    Services.obs.notifyObservers(xpi, "flush-cache-entry");
    xpi.remove(false);
  });

  let fileURI = Services.io.newFileURI(xpi);
  let jarURI = NetUtil.newURI(`jar:${fileURI.spec}!/webextension/`);

  let extension = new ExtensionData(jarURI);
  await extension.loadManifest();

  return extension;
}

add_task(async function testMissingDefaultLocale() {
  let extension = await generateAddon({
    "files": {
      "_locales/en_US/messages.json": {},
    },
  });

  equal(extension.errors.length, 0, "No errors reported");

  await extension.initAllLocales();

  equal(extension.errors.length, 1, "One error reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes('"default_locale" property is required'),
     "Got missing default_locale error");
});


add_task(async function testInvalidDefaultLocale() {
  let extension = await generateAddon({
    "manifest": {
      "default_locale": "en",
    },

    "files": {
      "_locales/en_US/messages.json": {},
    },
  });

  equal(extension.errors.length, 1, "One error reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes("Loading locale file _locales/en/messages.json"),
     "Got invalid default_locale error");

  await extension.initAllLocales();

  equal(extension.errors.length, 2, "Two errors reported");

  info(`Got error: ${extension.errors[1]}`);

  ok(extension.errors[1].includes('"default_locale" property must correspond'),
     "Got invalid default_locale error");
});


add_task(async function testUnexpectedDefaultLocale() {
  let extension = await generateAddon({
    "manifest": {
      "default_locale": "en_US",
    },
  });

  equal(extension.errors.length, 1, "One error reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes("Loading locale file _locales/en-US/messages.json"),
     "Got invalid default_locale error");

  await extension.initAllLocales();

  equal(extension.errors.length, 2, "One error reported");

  info(`Got error: ${extension.errors[1]}`);

  ok(extension.errors[1].includes('"default_locale" property must correspond'),
     "Got unexpected default_locale error");
});


add_task(async function testInvalidSyntax() {
  let extension = await generateAddon({
    "manifest": {
      "default_locale": "en_US",
    },

    "files": {
      "_locales/en_US/messages.json": '{foo: {message: "bar", description: "baz"}}',
    },
  });

  equal(extension.errors.length, 1, "No errors reported");

  info(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes("Loading locale file _locales\/en_US\/messages\.json: SyntaxError"),
     "Got syntax error");

  await extension.initAllLocales();

  equal(extension.errors.length, 2, "One error reported");

  info(`Got error: ${extension.errors[1]}`);

  ok(extension.errors[1].includes("Loading locale file _locales\/en_US\/messages\.json: SyntaxError"),
     "Got syntax error");
});
