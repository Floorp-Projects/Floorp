"use strict";

Cu.import("resource://gre/modules/Extension.jsm");

/* globals ExtensionData */

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

function* generateAddon(data) {
  let id = uuidGenerator.generateUUID().number;

  data = Object.assign({applications: {gecko: {id}}}, data);
  let xpi = Extension.generateXPI(data);
  do_register_cleanup(() => {
    Services.obs.notifyObservers(xpi, "flush-cache-entry", null);
    xpi.remove(false);
  });

  let fileURI = Services.io.newFileURI(xpi);
  let jarURI = NetUtil.newURI(`jar:${fileURI.spec}!/`);

  let extension = new ExtensionData(jarURI);
  yield extension.readManifest();

  return extension;
}

add_task(function* testMissingDefaultLocale() {
  let extension = yield generateAddon({
    "files": {
      "_locales/en_US/messages.json": {},
    },
  });

  equal(extension.errors.length, 0, "No errors reported");

  yield extension.initAllLocales();

  equal(extension.errors.length, 1, "One error reported");

  do_print(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes('"default_locale" property is required'),
     "Got missing default_locale error");
});


add_task(function* testInvalidDefaultLocale() {
  let extension = yield generateAddon({
    "manifest": {
      "default_locale": "en",
    },

    "files": {
      "_locales/en_US/messages.json": {},
    },
  });

  equal(extension.errors.length, 1, "One error reported");

  do_print(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes("Loading locale file _locales/en/messages.json"),
     "Got invalid default_locale error");

  yield extension.initAllLocales();

  equal(extension.errors.length, 2, "Two errors reported");

  do_print(`Got error: ${extension.errors[1]}`);

  ok(extension.errors[1].includes('"default_locale" property must correspond'),
     "Got invalid default_locale error");
});


add_task(function* testUnexpectedDefaultLocale() {
  let extension = yield generateAddon({
    "manifest": {
      "default_locale": "en_US",
    },
  });

  equal(extension.errors.length, 1, "One error reported");

  do_print(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes("Loading locale file _locales/en-US/messages.json"),
     "Got invalid default_locale error");

  yield extension.initAllLocales();

  equal(extension.errors.length, 2, "One error reported");

  do_print(`Got error: ${extension.errors[1]}`);

  ok(extension.errors[1].includes('"default_locale" property must correspond'),
     "Got unexpected default_locale error");
});


add_task(function* testInvalidSyntax() {
  let extension = yield generateAddon({
    "manifest": {
      "default_locale": "en_US",
    },

    "files": {
      "_locales/en_US/messages.json": '{foo: {message: "bar", description: "baz"}}',
    },
  });

  equal(extension.errors.length, 1, "No errors reported");

  do_print(`Got error: ${extension.errors[0]}`);

  ok(extension.errors[0].includes("Loading locale file _locales\/en_US\/messages\.json: SyntaxError"),
     "Got syntax error");

  yield extension.initAllLocales();

  equal(extension.errors.length, 2, "One error reported");

  do_print(`Got error: ${extension.errors[1]}`);

  ok(extension.errors[1].includes("Loading locale file _locales\/en_US\/messages\.json: SyntaxError"),
     "Got syntax error");
});
