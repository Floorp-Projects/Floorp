/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests ChromeManifestParser.js

Components.utils.import("resource://gre/modules/ChromeManifestParser.jsm");


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  startupManager();

  installAllFiles([do_get_addon("test_chromemanifest_1"),
                   do_get_addon("test_chromemanifest_2"),
                   do_get_addon("test_chromemanifest_3"),
                   do_get_addon("test_chromemanifest_4")],
                  function() {

    restartManager();
    run_test_1();
  });
}

function run_test_1() {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                              function([a1, a2, a3, a4]) {
    // addon1
    let a1Uri = a1.getResourceURI("/").spec;
    let expected = [
      {type: "content", baseURI: a1Uri, args: ["test-addon-1", "chrome/content"]},
      {type: "locale", baseURI: a1Uri, args: ["test-addon-1", "en-US", "locale/en-US"]},
      {type: "locale", baseURI: a1Uri, args: ["test-addon-1", "fr-FR", "locale/fr-FR"]},
      {type: "overlay", baseURI: a1Uri, args: ["chrome://browser/content/browser.xul", "chrome://test-addon-1/content/overlay.xul"]}
    ];
    let manifestURI = a1.getResourceURI("chrome.manifest");
    let manifest = ChromeManifestParser.parseSync(manifestURI);

    do_check_true(Array.isArray(manifest));
    do_check_eq(manifest.length, expected.length);
    for (let i = 0; i < manifest.length; i++) {
      do_check_eq(JSON.stringify(manifest[i]), JSON.stringify(expected[i]));
    }

    // addon2
    let a2Uri = a2.getResourceURI("/").spec;
    expected = [
      {type: "content", baseURI: a2Uri, args: ["test-addon-1", "chrome/content"]},
      {type: "locale", baseURI: a2Uri, args: ["test-addon-1", "en-US", "locale/en-US"]},
      {type: "locale", baseURI: a2Uri, args: ["test-addon-1", "fr-FR", "locale/fr-FR"]},
      {type: "overlay", baseURI: a2Uri, args: ["chrome://browser/content/browser.xul", "chrome://test-addon-1/content/overlay.xul"]},
      {type: "binary-component", baseURI: a2Uri, args: ["components/something.so"]}
    ];
    manifestURI = a2.getResourceURI("chrome.manifest");
    manifest = ChromeManifestParser.parseSync(manifestURI);

    do_check_true(Array.isArray(manifest));
    do_check_eq(manifest.length, expected.length);
    for (let i = 0; i < manifest.length; i++) {
      do_check_eq(JSON.stringify(manifest[i]), JSON.stringify(expected[i]));
    }

    // addon3
    let a3Uri = a3.getResourceURI("/").spec;
    expected = [
      {type: "content", baseURI: a3Uri, args: ["test-addon-1", "chrome/content"]},
      {type: "locale", baseURI: a3Uri, args: ["test-addon-1", "en-US", "locale/en-US"]},
      {type: "locale", baseURI: a3Uri, args: ["test-addon-1", "fr-FR", "locale/fr-FR"]},
      {type: "overlay", baseURI: a3Uri, args: ["chrome://browser/content/browser.xul", "chrome://test-addon-1/content/overlay.xul"]},
      {type: "binary-component", baseURI: a3Uri, args: ["components/something.so"]},
      {type: "locale", baseURI: "jar:" + a3.getResourceURI("/inner.jar").spec + "!/", args: ["test-addon-1", "en-NZ", "locale/en-NZ"]},
    ];
    manifestURI = a3.getResourceURI("chrome.manifest");
    manifest = ChromeManifestParser.parseSync(manifestURI);

    do_check_true(Array.isArray(manifest));
    do_check_eq(manifest.length, expected.length);
    for (let i = 0; i < manifest.length; i++) {
      do_check_eq(JSON.stringify(manifest[i]), JSON.stringify(expected[i]));
    }

    // addon4
    let a4Uri = a4.getResourceURI("/").spec;
    expected = [
      {type: "content", baseURI: a4Uri, args: ["test-addon-1", "chrome/content"]},
      {type: "locale", baseURI: a4Uri, args: ["test-addon-1", "en-US", "locale/en-US"]},
      {type: "locale", baseURI: a4Uri, args: ["test-addon-1", "fr-FR", "locale/fr-FR"]},
      {type: "overlay", baseURI: a4Uri, args: ["chrome://browser/content/browser.xul", "chrome://test-addon-1/content/overlay.xul"]},
      {type: "binary-component", baseURI: a4.getResourceURI("components/").spec, args: ["mycomponent.dll"]},
      {type: "binary-component", baseURI: a4.getResourceURI("components/other/").spec, args: ["thermalnuclearwar.dll"]}
    ];
    manifestURI = a4.getResourceURI("chrome.manifest");
    manifest = ChromeManifestParser.parseSync(manifestURI);

    do_check_true(Array.isArray(manifest));
    do_check_eq(manifest.length, expected.length);
    for (let i = 0; i < manifest.length; i++) {
      do_check_eq(JSON.stringify(manifest[i]), JSON.stringify(expected[i]));
    }

    do_execute_soon(do_test_finished);
  });
}
