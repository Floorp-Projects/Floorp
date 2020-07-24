"use strict";

function getFileURI(path) {
  let file = do_get_file(".");
  file.append(path);
  return Services.io.newFileURI(file);
}

add_task(async function() {
  const registry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );

  let file1 = getFileURI("file1");
  let file2 = getFileURI("file2");

  let uri1 = getFileURI("chrome.manifest");
  let uri2 = getFileURI("manifest.json");

  let overrideURL = Services.io.newURI("chrome://global/content/foo");
  let contentURL = Services.io.newURI("chrome://test/content/foo");
  let localeURL = Services.io.newURI("chrome://global/locale/foo");

  let origOverrideURL = registry.convertChromeURL(overrideURL);
  let origLocaleURL = registry.convertChromeURL(localeURL);

  let entry1 = aomStartup.registerChrome(uri1, [
    ["override", "chrome://global/content/foo", file1.spec],
    ["content", "test", file2.spec + "/"],
    ["locale", "global", "en-US", file2.spec + "/"],
  ]);

  let entry2 = aomStartup.registerChrome(uri2, [
    ["override", "chrome://global/content/foo", file2.spec],
    ["content", "test", file1.spec + "/"],
    ["locale", "global", "en-US", file1.spec + "/"],
  ]);

  // Initially, the second entry should override the first.
  equal(registry.convertChromeURL(overrideURL).spec, file2.spec);
  let file = file1.spec + "/foo";
  equal(registry.convertChromeURL(contentURL).spec, file);
  equal(registry.convertChromeURL(localeURL).spec, file);

  // After destroying the second entry, the first entry should now take
  // precedence.
  entry2.destruct();
  equal(registry.convertChromeURL(overrideURL).spec, file1.spec);
  file = file2.spec + "/foo";
  equal(registry.convertChromeURL(contentURL).spec, file);
  equal(registry.convertChromeURL(localeURL).spec, file);

  // After dropping the reference to the first entry and allowing it to
  // be GCed, we should be back to the original entries.
  entry1 = null; // eslint-disable-line no-unused-vars
  Cu.forceGC();
  Cu.forceCC();
  equal(registry.convertChromeURL(overrideURL).spec, origOverrideURL.spec);
  equal(registry.convertChromeURL(localeURL).spec, origLocaleURL.spec);
  Assert.throws(
    () => registry.convertChromeURL(contentURL),
    e => e.result == Cr.NS_ERROR_FILE_NOT_FOUND,
    "chrome://test/ should no longer be registered"
  );
});

add_task(async function() {
  const INVALID_VALUES = [
    {},
    "foo",
    ["foo"],
    [{}],
    [[]],
    [["locale", "global"]],
    [["locale", "global", "en", "foo", "foo"]],
    [["override", "en"]],
    [["override", "en", "US", "OR"]],
  ];

  let uri = getFileURI("chrome.manifest");
  for (let arg of INVALID_VALUES) {
    Assert.throws(
      () => aomStartup.registerChrome(uri, arg),
      e => e.result == Cr.NS_ERROR_INVALID_ARG,
      `Arg ${uneval(arg)} should throw`
    );
  }
});
