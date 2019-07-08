"use strict";

function delay(time) {
  return new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, time);
  });
}

const { Extension } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm"
);

add_task(async function test_startup_request_handler() {
  const ID = "request-startup@xpcshell.mozilla.org";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: ID } },
    },

    files: {
      "meh.txt": "Meh.",
    },
  });

  let ready = false;
  let resolvePromise;
  let promise = new Promise(resolve => {
    resolvePromise = resolve;
  });
  promise.then(() => {
    ready = true;
  });

  let origInitLocale = Extension.prototype.initLocale;
  Extension.prototype.initLocale = async function initLocale() {
    await promise;
    return origInitLocale.call(this);
  };

  let startupPromise = extension.startup();

  await delay(0);
  let policy = WebExtensionPolicy.getByID(ID);
  let url = policy.getURL("meh.txt");

  let resp = ExtensionTestUtils.fetch(url, url);
  resp.then(() => {
    ok(ready, "Shouldn't get response before extension is ready");
  });

  await delay(2000);

  resolvePromise();
  await startupPromise;

  let body = await resp;
  equal(body, "Meh.", "Got the correct response");

  await extension.unload();

  Extension.prototype.initLocale = origInitLocale;
});
