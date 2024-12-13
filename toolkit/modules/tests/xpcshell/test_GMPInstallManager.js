/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

const URL_HOST = "http://localhost";
const PR_USEC_PER_MSEC = 1000;

const { GMPExtractor, GMPInstallManager } = ChromeUtils.importESModule(
  "resource://gre/modules/GMPInstallManager.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { Preferences } = ChromeUtils.importESModule(
  "resource://gre/modules/Preferences.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { UpdateUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/UpdateUtils.sys.mjs"
);
const { GMPPrefs, OPEN_H264_ID } = ChromeUtils.importESModule(
  "resource://gre/modules/GMPUtils.sys.mjs"
);
const { ProductAddonCheckerTestUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/ProductAddonChecker.sys.mjs"
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
Services.prefs.setBoolPref("media.gmp-manager.updateEnabled", true);
// Gather the telemetry even where the probes don't exist (i.e. Thunderbird).
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  Services.prefs.clearUserPref("media.gmp-manager.updateEnabled");
  Services.prefs.clearUserPref(
    "toolkit.telemetry.testing.overrideProductsCheck"
  );
});
// Most tests do no handle the machinery for content signatures, so let
// specific tests that need it turn it on as needed.
Preferences.set("media.gmp-manager.checkContentSignature", false);

do_get_profile();

add_task(function test_setup() {
  // We should already have a profile from `do_get_profile`, but also need
  // FOG to be setup for tests that touch it/Glean.
  Services.fog.initializeFOG();
});

function run_test() {
  Preferences.set("media.gmp.log.dump", true);
  Preferences.set("media.gmp.log.level", 0);
  run_next_test();
}

/**
 * Tests that the helper used for preferences works correctly
 */
add_task(async function test_prefs() {
  let addon1 = "addon1",
    addon2 = "addon2";

  GMPPrefs.setString(GMPPrefs.KEY_URL, "http://not-really-used");
  GMPPrefs.setString(GMPPrefs.KEY_URL_OVERRIDE, "http://not-really-used-2");
  GMPPrefs.setInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, 1, addon1);
  GMPPrefs.setString(GMPPrefs.KEY_PLUGIN_VERSION, "2", addon1);
  GMPPrefs.setInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, 3, addon2);
  GMPPrefs.setInt(GMPPrefs.KEY_PLUGIN_VERSION, 4, addon2);
  GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, false, addon2);
  GMPPrefs.setBool(GMPPrefs.KEY_CERT_CHECKATTRS, true);
  GMPPrefs.setString(GMPPrefs.KEY_PLUGIN_HASHVALUE, "5", addon1);

  Assert.equal(GMPPrefs.getString(GMPPrefs.KEY_URL), "http://not-really-used");
  Assert.equal(
    GMPPrefs.getString(GMPPrefs.KEY_URL_OVERRIDE),
    "http://not-really-used-2"
  );
  Assert.equal(GMPPrefs.getInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, "", addon1), 1);
  Assert.equal(
    GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION, "", addon1),
    "2"
  );
  Assert.equal(GMPPrefs.getInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, "", addon2), 3);
  Assert.equal(GMPPrefs.getInt(GMPPrefs.KEY_PLUGIN_VERSION, "", addon2), 4);
  Assert.equal(
    GMPPrefs.getBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, undefined, addon2),
    false
  );
  Assert.ok(GMPPrefs.getBool(GMPPrefs.KEY_CERT_CHECKATTRS));
  GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, addon2);
  Assert.equal(
    GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_HASHVALUE, "", addon1),
    "5"
  );
});

/**
 * Tests that an uninit without a check works fine
 */
add_task(async function test_checkForAddons_uninitWithoutCheck() {
  let installManager = new GMPInstallManager();
  installManager.uninit();
});

/**
 * Tests that an uninit without an install works fine
 */
add_test(function test_checkForAddons_uninitWithoutInstall() {
  let myRequest = new mockRequest(200, "");
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that no response returned rejects
 */
add_test(function test_checkForAddons_noResponse() {
  let myRequest = new mockRequest(200, "");
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that no addons element returned resolves with no addons
 */
add_task(async function test_checkForAddons_noAddonsElement() {
  let myRequest = new mockRequest(200, "<updates></updates>");
  let installManager = new GMPInstallManager();
  let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  Assert.equal(res.addons.length, 0);
  installManager.uninit();
});

/**
 * Tests that empty addons element returned resolves with no addons
 */
add_task(async function test_checkForAddons_emptyAddonsElement() {
  let myRequest = new mockRequest(200, "<updates><addons/></updates>");
  let installManager = new GMPInstallManager();
  let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  Assert.equal(res.addons.length, 0);
  installManager.uninit();
});

/**
 * Tests that a response with the wrong root element rejects
 */
add_test(function test_checkForAddons_wrongResponseXML() {
  let myRequest = new mockRequest(
    200,
    "<digits_of_pi>3.141592653589793....</digits_of_pi>"
  );
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that a 404 error works as expected
 */
add_test(function test_checkForAddons_404Error() {
  let myRequest = new mockRequest(404, "");
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that a xhr/ServiceRequest abort() works as expected
 */
add_test(function test_checkForAddons_abort() {
  let overriddenServiceRequest = new mockRequest(200, "", {
    dropRequest: true,
  });
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    overriddenServiceRequest,
    () => installManager.checkForAddons()
  );

  // Since the ServiceRequest is created in checkForAddons asynchronously,
  // we need to delay aborting till the request is running.
  // Since checkForAddons returns a Promise already only after
  // the abort is triggered, we can't use that, and instead
  // we'll use a fake timer.
  setTimeout(() => {
    overriddenServiceRequest.abort();
  }, 100);

  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that a defensive timeout works as expected
 */
add_test(function test_checkForAddons_timeout() {
  let myRequest = new mockRequest(200, "", {
    dropRequest: true,
    timeout: true,
  });
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that we throw correctly in case of ssl certification error.
 */
add_test(function test_checkForAddons_bad_ssl() {
  //
  // Add random stuff that cause CertUtil to require https.
  //
  let PREF_KEY_URL_OVERRIDE_BACKUP = Preferences.get(
    GMPPrefs.KEY_URL_OVERRIDE,
    ""
  );
  Preferences.reset(GMPPrefs.KEY_URL_OVERRIDE);

  let CERTS_BRANCH_DOT_ONE = GMPPrefs.KEY_CERTS_BRANCH + ".1";
  let PREF_CERTS_BRANCH_DOT_ONE_BACKUP = Preferences.get(
    CERTS_BRANCH_DOT_ONE,
    ""
  );
  Services.prefs.setCharPref(CERTS_BRANCH_DOT_ONE, "funky value");

  let myRequest = new mockRequest(200, "");
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    if (PREF_KEY_URL_OVERRIDE_BACKUP) {
      Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, PREF_KEY_URL_OVERRIDE_BACKUP);
    }
    if (PREF_CERTS_BRANCH_DOT_ONE_BACKUP) {
      Preferences.set(CERTS_BRANCH_DOT_ONE, PREF_CERTS_BRANCH_DOT_ONE_BACKUP);
    }
    run_next_test();
  });
});

/**
 * Tests that gettinga a funky non XML response works as expected
 */
add_test(function test_checkForAddons_notXML() {
  let myRequest = new mockRequest(200, "3.141592653589793....");
  let installManager = new GMPInstallManager();
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );

  promise.then(res => {
    Assert.equal(res.addons.length, 2);
    for (let addon of res.addons) {
      Assert.ok(addon.usedFallback);
    }
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that getting a response with a single addon works as expected
 */
add_task(async function test_checkForAddons_singleAddon() {
  let responseXML =
    '<?xml version="1.0"?>' +
    "<updates>" +
    "    <addons>" +
    '        <addon id="gmp-gmpopenh264"' +
    '               URL="http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip"' +
    '               hashFunction="sha256"' +
    '               hashValue="1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
    '               version="1.1"/>' +
    "  </addons>" +
    "</updates>";
  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  Assert.equal(res.addons.length, 1);
  let gmpAddon = res.addons[0];
  Assert.equal(gmpAddon.id, "gmp-gmpopenh264");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(
    gmpAddon.hashValue,
    "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"
  );
  Assert.equal(gmpAddon.version, "1.1");
  Assert.ok(gmpAddon.isValid);
  Assert.ok(!gmpAddon.isInstalled);
  installManager.uninit();
});

/**
 * Tests that getting a response with a single addon with the optional size
 * attribute parses as expected.
 */
add_task(async function test_checkForAddons_singleAddonWithSize() {
  let responseXML =
    '<?xml version="1.0"?>' +
    "<updates>" +
    "    <addons>" +
    '        <addon id="openh264-plugin-no-at-symbol"' +
    '               URL="http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip"' +
    '               hashFunction="sha256"' +
    '               size="42"' +
    '               hashValue="1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
    '               version="1.1"/>' +
    "  </addons>" +
    "</updates>";
  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  Assert.equal(res.addons.length, 1);
  let gmpAddon = res.addons[0];
  Assert.equal(gmpAddon.id, "openh264-plugin-no-at-symbol");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(
    gmpAddon.hashValue,
    "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"
  );
  Assert.equal(gmpAddon.size, 42);
  Assert.equal(gmpAddon.version, "1.1");
  Assert.ok(gmpAddon.isValid);
  Assert.ok(!gmpAddon.isInstalled);
  installManager.uninit();
});

/**
 * Tests that checking for multiple addons work correctly.
 * Also tests that invalid addons work correctly.
 */
add_task(
  async function test_checkForAddons_multipleAddonNoUpdatesSomeInvalid() {
    let responseXML =
      '<?xml version="1.0"?>' +
      "<updates>" +
      "    <addons>" +
      // valid openh264
      '        <addon id="gmp-gmpopenh264"' +
      '               URL="http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip"' +
      '               hashFunction="sha256"' +
      '               hashValue="1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               version="1.1"/>' +
      // valid not openh264
      '        <addon id="NOT-gmp-gmpopenh264"' +
      '               URL="http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"' +
      '               hashFunction="sha512"' +
      '               hashValue="141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               version="9.1"/>' +
      // noid
      '        <addon notid="NOT-gmp-gmpopenh264"' +
      '               URL="http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"' +
      '               hashFunction="sha512"' +
      '               hashValue="141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               version="9.1"/>' +
      // no URL
      '        <addon id="NOT-gmp-gmpopenh264"' +
      '               notURL="http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"' +
      '               hashFunction="sha512"' +
      '               hashValue="141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               version="9.1"/>' +
      // no hash function
      '        <addon id="NOT-gmp-gmpopenh264"' +
      '               URL="http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"' +
      '               nothashFunction="sha512"' +
      '               hashValue="141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               version="9.1"/>' +
      // no hash function
      '        <addon id="NOT-gmp-gmpopenh264"' +
      '               URL="http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"' +
      '               hashFunction="sha512"' +
      '               nothashValue="141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               version="9.1"/>' +
      // not version
      '        <addon id="NOT-gmp-gmpopenh264"' +
      '               URL="http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"' +
      '               hashFunction="sha512"' +
      '               hashValue="141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
      '               notversion="9.1"/>' +
      "  </addons>" +
      "</updates>";
    let myRequest = new mockRequest(200, responseXML);
    let installManager = new GMPInstallManager();
    let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
      myRequest,
      () => installManager.checkForAddons()
    );
    Assert.equal(res.addons.length, 7);
    let gmpAddon = res.addons[0];
    Assert.equal(gmpAddon.id, "gmp-gmpopenh264");
    Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
    Assert.equal(gmpAddon.hashFunction, "sha256");
    Assert.equal(
      gmpAddon.hashValue,
      "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"
    );
    Assert.equal(gmpAddon.version, "1.1");
    Assert.ok(gmpAddon.isValid);
    Assert.ok(!gmpAddon.isInstalled);

    gmpAddon = res.addons[1];
    Assert.equal(gmpAddon.id, "NOT-gmp-gmpopenh264");
    Assert.equal(
      gmpAddon.URL,
      "http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip"
    );
    Assert.equal(gmpAddon.hashFunction, "sha512");
    Assert.equal(
      gmpAddon.hashValue,
      "141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"
    );
    Assert.equal(gmpAddon.version, "9.1");
    Assert.ok(gmpAddon.isValid);
    Assert.ok(!gmpAddon.isInstalled);

    for (let i = 2; i < res.addons.length; i++) {
      Assert.ok(!res.addons[i].isValid);
      Assert.ok(!res.addons[i].isInstalled);
    }
    installManager.uninit();
  }
);

/**
 * Tests that checking for addons when there are also updates available
 * works as expected.
 */
add_task(async function test_checkForAddons_updatesWithAddons() {
  let responseXML =
    '<?xml version="1.0"?>' +
    "    <updates>" +
    '        <update type="minor" displayVersion="33.0a1" appVersion="33.0a1" platformVersion="33.0a1" buildID="20140628030201">' +
    '        <patch type="complete" URL="http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/2014/06/2014-06-28-03-02-01-mozilla-central/firefox-33.0a1.en-US.mac.complete.mar" hashFunction="sha512" hashValue="f3f90d71dff03ae81def80e64bba3e4569da99c9e15269f731c2b167c4fc30b3aed9f5fee81c19614120230ca333e73a5e7def1b8e45d03135b2069c26736219" size="85249896"/>' +
    "    </update>" +
    "    <addons>" +
    '        <addon id="gmp-gmpopenh264"' +
    '               URL="http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip"' +
    '               hashFunction="sha256"' +
    '               hashValue="1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
    '               version="1.1"/>' +
    "  </addons>" +
    "</updates>";
  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  Assert.equal(res.addons.length, 1);
  let gmpAddon = res.addons[0];
  Assert.equal(gmpAddon.id, "gmp-gmpopenh264");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(
    gmpAddon.hashValue,
    "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"
  );
  Assert.equal(gmpAddon.version, "1.1");
  Assert.ok(gmpAddon.isValid);
  Assert.ok(!gmpAddon.isInstalled);
  installManager.uninit();
});

/**
 * Tests that checkForAddons() works as expected when content signature
 * checking is enabled and the signature check passes.
 */
add_task(async function test_checkForAddons_contentSignatureSuccess() {
  const previousUrlOverride = setupContentSigTestPrefs();

  const xmlFetchResultHistogram = resetGmpTelemetryAndGetHistogram();

  const testServerInfo = getTestServerForContentSignatureTests();
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.validUpdateUri);

  let installManager = new GMPInstallManager();
  try {
    let res = await installManager.checkForAddons();
    Assert.ok(true, "checkForAddons should succeed");

    // Smoke test the results are as expected.
    // If the checkForAddons fails we'll get a fallback config,
    // so we'll get incorrect addons and these asserts will fail.
    Assert.equal(res.addons.length, 7);
    Assert.equal(res.addons[0].id, "test1");
    Assert.equal(res.addons[0].usedFallback, false);
    Assert.deepEqual(res.addons[0].mirrorURLs, []);
    Assert.equal(res.addons[1].id, "test2");
    Assert.equal(res.addons[1].usedFallback, false);
    Assert.deepEqual(res.addons[1].mirrorURLs, []);
    Assert.equal(res.addons[2].id, "test3");
    Assert.equal(res.addons[2].usedFallback, false);
    Assert.deepEqual(res.addons[2].mirrorURLs, []);
    Assert.equal(res.addons[3].id, "test4");
    Assert.equal(res.addons[3].usedFallback, false);
    Assert.deepEqual(res.addons[3].mirrorURLs, []);
    Assert.equal(res.addons[4].id, undefined);
    Assert.equal(res.addons[4].usedFallback, false);
    Assert.deepEqual(res.addons[4].mirrorURLs, []);
    Assert.equal(res.addons[5].id, "test6");
    Assert.equal(res.addons[5].usedFallback, false);
    Assert.deepEqual(res.addons[5].mirrorURLs, [
      "http://alt.example.com/test6.xpi",
    ]);
    Assert.equal(res.addons[5].mirrorURLs.length, 1);
    Assert.equal(res.addons[6].id, "test7");
    Assert.equal(res.addons[6].usedFallback, false);
    Assert.deepEqual(res.addons[6].mirrorURLs, [
      "http://alt.example.com/test7.xpi",
      "http://alt2.example.com/test7.xpi",
    ]);
  } catch (e) {
    Assert.ok(false, "checkForAddons should succeed");
  }

  // # Ok content sig fetches should be 1, all others should be 0.
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 2, 1);
  // Test that glean has 1 success for content sig and no other metrics.
  const expectedGleanValues = {
    cert_pin_success: 0,
    cert_pin_net_request_error: 0,
    cert_pin_net_timeout: 0,
    cert_pin_abort: 0,
    cert_pin_missing_data: 0,
    cert_pin_failed: 0,
    cert_pin_invalid: 0,
    cert_pin_unknown_error: 0,
    content_sig_success: 1,
    content_sig_net_request_error: 0,
    content_sig_net_timeout: 0,
    content_sig_abort: 0,
    content_sig_missing_data: 0,
    content_sig_failed: 0,
    content_sig_invalid: 0,
    content_sig_unknown_error: 0,
  };
  checkGleanMetricCounts(expectedGleanValues);

  revertContentSigTestPrefs(previousUrlOverride);
});

/**
 * Tests that checkForAddons() works as expected when content signature
 * checking is enabled and the check fails.
 */
add_task(async function test_checkForAddons_contentSignatureFailure() {
  const previousUrlOverride = setupContentSigTestPrefs();

  const xmlFetchResultHistogram = resetGmpTelemetryAndGetHistogram();

  const testServerInfo = getTestServerForContentSignatureTests();
  Preferences.set(
    GMPPrefs.KEY_URL_OVERRIDE,
    testServerInfo.missingContentSigUri
  );

  let installManager = new GMPInstallManager();
  try {
    let res = await installManager.checkForAddons();
    Assert.ok(true, "checkForAddons should succeed");

    // Smoke test the results are as expected.
    // Check addons will succeed above, but it will have fallen back to local
    // config. So the results will not be those from the HTTP server.
    // Some platforms don't have fallback config for all GMPs, but we should
    // always get at least 1.
    Assert.greaterOrEqual(res.addons.length, 1);
    if (res.addons.length == 1) {
      Assert.equal(res.addons[0].id, "gmp-widevinecdm");
      Assert.equal(res.addons[0].usedFallback, true);
      Assert.ok(res.addons[0].URL.startsWith("https://edgedl.me.gvt1.com"));
      Assert.equal(res.addons[0].mirrorURLs.length, 1);
      Assert.ok(
        res.addons[0].mirrorURLs[0].startsWith("https://www.google.com")
      );
    } else {
      Assert.equal(res.addons[0].id, "gmp-gmpopenh264");
      Assert.equal(res.addons[0].usedFallback, true);
      Assert.ok(
        res.addons[0].URL.startsWith("http://ciscobinary.openh264.org")
      );
      Assert.deepEqual(res.addons[0].mirrorURLs, []);
      Assert.equal(res.addons[1].id, "gmp-widevinecdm");
      Assert.equal(res.addons[1].usedFallback, true);
      Assert.ok(res.addons[1].URL.startsWith("https://edgedl.me.gvt1.com"));
      Assert.equal(res.addons[1].mirrorURLs.length, 1);
      Assert.ok(
        res.addons[1].mirrorURLs[0].startsWith("https://www.google.com")
      );
      if (res.addons.length >= 3) {
        Assert.equal(res.addons[2].id, "gmp-widevinecdm-l1");
        Assert.equal(res.addons[2].usedFallback, true);
        Assert.ok(res.addons[2].URL.startsWith("https://edgedl.me.gvt1.com"));
        Assert.equal(res.addons[2].mirrorURLs.length, 1);
        Assert.ok(
          res.addons[2].mirrorURLs[0].startsWith("https://www.google.com")
        );
      }
    }
  } catch (e) {
    Assert.ok(false, "checkForAddons should succeed");
  }

  // # Failed content sig fetches should be 1, all others should be 0.
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 1);
  // Glean values should reflect the content sig algo failed.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_missing_data.testGetValue(),
    1
  );

  // Check further failure cases. We've already smoke tested the results above,
  // don't bother doing so again.

  // Fail due to bad content signature.
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.badContentSigUri);
  await installManager.checkForAddons();
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 2);
  // ... and it should be due to the signature being bad, which causes
  // verification to fail.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_failed.testGetValue(),
    1
  );

  // Fail due to bad invalid content signature.
  Preferences.set(
    GMPPrefs.KEY_URL_OVERRIDE,
    testServerInfo.invalidContentSigUri
  );
  await installManager.checkForAddons();
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 3);
  // ... and it should be due to the signature being invalid.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_invalid.testGetValue(),
    1
  );

  // Fail by pointing to a bad URL.
  Preferences.set(
    GMPPrefs.KEY_URL_OVERRIDE,
    "https://this.url.doesnt/go/anywhere"
  );
  await installManager.checkForAddons();
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 4);
  // ... and it should be due to a bad request.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_net_request_error.testGetValue(),
    1
  );

  // Fail via timeout. This case uses our mock machinery in order to abort the
  // request, as I (:bryce) couldn't figure out a nice way to do it with the
  // HttpServer.
  let overriddenServiceRequest = new mockRequest(200, "", {
    dropRequest: true,
    timeout: true,
  });
  await ProductAddonCheckerTestUtils.overrideServiceRequest(
    overriddenServiceRequest,
    () => installManager.checkForAddons()
  );
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 5);
  // ... and it should be due to a timeout.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_net_timeout.testGetValue(),
    1
  );

  // Fail via abort. This case uses our mock machinery in order to abort the
  // request, as I (:bryce) couldn't figure out a nice way to do it with the
  // HttpServer.
  overriddenServiceRequest = new mockRequest(200, "", {
    dropRequest: true,
  });
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    overriddenServiceRequest,
    () => installManager.checkForAddons()
  );
  setTimeout(() => {
    overriddenServiceRequest.abort();
  }, 100);
  await promise;
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 6);
  // ... and it should be due to an abort.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_abort.testGetValue(),
    1
  );

  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.badXmlUri);
  await installManager.checkForAddons();
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 7);
  // ... and it should be due to the xml response being unrecognized.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_xml_parse_error.testGetValue(),
    1
  );

  // Fail via bad request during the x5u look up.
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.badX5uRequestUri);
  await installManager.checkForAddons();
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 8);
  // ... and it should be due to a bad request.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_net_request_error.testGetValue(),
    2
  );

  // Fail by timing out during the x5u look up.
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.x5uTimeoutUri);
  // We need to expose this promise back to the server so it can handle
  // setting up a mock request in the middle of checking for addons.
  testServerInfo.promiseHolder.installPromise = installManager.checkForAddons();
  await testServerInfo.promiseHolder.installPromise;
  // We wait sequentially because serverPromise won't be set until the server
  // receives our request.
  await testServerInfo.promiseHolder.serverPromise;
  delete testServerInfo.promiseHolder.installPromise;
  delete testServerInfo.promiseHolder.serverPromise;
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 9);
  // ... and it should be due to a timeout.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_net_timeout.testGetValue(),
    2
  );

  // Fail by aborting during the x5u look up.
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.x5uAbortUri);
  // We need to expose this promise back to the server so it can handle
  // setting up a mock request in the middle of checking for addons.
  testServerInfo.promiseHolder.installPromise = installManager.checkForAddons();
  await testServerInfo.promiseHolder.installPromise;
  // We wait sequentially because serverPromise won't be set until the server
  // receives our request.
  await testServerInfo.promiseHolder.serverPromise;
  delete testServerInfo.promiseHolder.installPromise;
  delete testServerInfo.promiseHolder.serverPromise;
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 10);
  // ... and it should be due to an abort.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.content_sig_abort.testGetValue(),
    2
  );

  // Check all glean metrics have expected values at test end.
  const expectedGleanValues = {
    cert_pin_success: 0,
    cert_pin_net_request_error: 0,
    cert_pin_net_timeout: 0,
    cert_pin_abort: 0,
    cert_pin_missing_data: 0,
    cert_pin_failed: 0,
    cert_pin_invalid: 0,
    cert_pin_xml_parse_error: 0,
    cert_pin_unknown_error: 0,
    content_sig_success: 0,
    content_sig_net_request_error: 2,
    content_sig_net_timeout: 2,
    content_sig_abort: 2,
    content_sig_missing_data: 1,
    content_sig_failed: 1,
    content_sig_invalid: 1,
    content_sig_xml_parse_error: 1,
    content_sig_unknown_error: 0,
  };
  checkGleanMetricCounts(expectedGleanValues);

  revertContentSigTestPrefs(previousUrlOverride);
});

/**
 * Tests that the signature verification URL is as expected.
 */
add_task(async function test_checkForAddons_get_verifier_url() {
  const previousUrlOverride = setupContentSigTestPrefs();

  let installManager = new GMPInstallManager();
  // checkForAddons() calls _getContentSignatureRootForURL() with the return
  // value of _getURL(), which is effectively KEY_URL_OVERRIDE or KEY_URL
  // followed by some normalization.
  const rootForUrl = async () => {
    const url = await installManager._getURL();
    return installManager._getContentSignatureRootForURL(url);
  };

  Assert.equal(
    await rootForUrl(),
    Ci.nsIX509CertDB.AppXPCShellRoot,
    "XPCShell root used by default in xpcshell test"
  );

  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const defaultUrl = defaultPrefs.getStringPref(GMPPrefs.KEY_URL);
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, defaultUrl);
  Assert.equal(
    await rootForUrl(),
    Ci.nsIContentSignatureVerifier.ContentSignatureProdRoot,
    "Production cert should be used for the default Balrog URL: " + defaultUrl
  );

  // The current Balrog endpoint is at aus5.mozilla.org. Confirm that the prod
  // cert is used even if we bump the version (e.g. aus6):
  const potentialProdUrl = "https://aus1337.mozilla.org/potential/prod/URL";
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, potentialProdUrl);
  Assert.equal(
    await rootForUrl(),
    Ci.nsIContentSignatureVerifier.ContentSignatureProdRoot,
    "Production cert should be used for: " + potentialProdUrl
  );

  // Stage URL documented at https://mozilla-balrog.readthedocs.io/en/latest/infrastructure.html
  const stageUrl = "https://stage.balrog.nonprod.cloudops.mozgcp.net/etc.";
  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, stageUrl);
  Assert.equal(
    await rootForUrl(),
    Ci.nsIContentSignatureVerifier.ContentSignatureStageRoot,
    "Stage cert should be used with the stage URL: " + stageUrl
  );

  installManager.uninit();

  revertContentSigTestPrefs(previousUrlOverride);
});

/**
 * Tests that checkForAddons() works as expected when certificate pinning
 * checking is enabled. We plan to move away from cert pinning in favor of
 * content signature checks, but part of doing this is comparing the telemetry
 * from both methods. We want test coverage to ensure the telemetry is being
 * gathered for cert pinning failures correctly before we remove the code.
 */
add_task(async function test_checkForAddons_telemetry_certPinning() {
  // Grab state so we can restore it at the end of the test.
  const previousUrlOverride = Preferences.get(GMPPrefs.KEY_URL_OVERRIDE, "");

  let xmlFetchResultHistogram = resetGmpTelemetryAndGetHistogram();

  // Re-use the content-sig test server config. We're not going to need any of
  // the content signature specific config but this gives us a server to get
  // update.xml files from, and also tests that cert pinning doesn't break even
  // if we're getting content sig headers sent.
  const testServerInfo = getTestServerForContentSignatureTests();

  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, testServerInfo.validUpdateUri);

  let installManager = new GMPInstallManager();
  try {
    // This should work because once we override the GMP URL, no cert pin
    // checks are actually done. I.e. we don't need to do any pinning in
    // the test, just use a valid URL.
    await installManager.checkForAddons();
    Assert.ok(true, "checkForAddons should succeed");
  } catch (e) {
    Assert.ok(false, "checkForAddons should succeed");
  }

  // # Ok cert pin fetches should be 1, all others should be 0.
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 0, 1);
  // Glean values should reflect the same.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.cert_pin_success.testGetValue(),
    1
  );

  // Reset the histogram because we want to check a different index.
  xmlFetchResultHistogram = TelemetryTestUtils.getAndClearHistogram(
    "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
  );
  // Fail by pointing to a bad URL.
  Preferences.set(
    GMPPrefs.KEY_URL_OVERRIDE,
    "https://this.url.doesnt/go/anywhere"
  );
  await installManager.checkForAddons();
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 1, 1);
  // ... and it should be due to a bad request.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.cert_pin_net_request_error.testGetValue(),
    1
  );

  // Fail via timeout. This case uses our mock machinery in order to abort the
  // request, as I (:bryce) couldn't figure out a nice way to do it with the
  // HttpServer.
  let overriddenServiceRequest = new mockRequest(200, "", {
    dropRequest: true,
    timeout: true,
  });
  await ProductAddonCheckerTestUtils.overrideServiceRequest(
    overriddenServiceRequest,
    () => installManager.checkForAddons()
  );
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 1, 2);
  // ... and it should be due to a timeout.
  Assert.equal(
    Glean.gmp.updateXmlFetchResult.cert_pin_net_timeout.testGetValue(),
    1
  );

  // Fail via abort. This case uses our mock machinery in order to abort the
  // request, as I (:bryce) couldn't figure out a nice way to do it with the
  // HttpServer.
  overriddenServiceRequest = new mockRequest(200, "", {
    dropRequest: true,
  });
  let promise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    overriddenServiceRequest,
    () => installManager.checkForAddons()
  );
  setTimeout(() => {
    overriddenServiceRequest.abort();
  }, 100);
  await promise;
  // Should have another failure...
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 1, 3);
  // ... and it should be due to an abort.
  Assert.equal(Glean.gmp.updateXmlFetchResult.cert_pin_abort.testGetValue(), 1);

  // Check all glean metrics have expected values at test end.
  const expectedGleanValues = {
    cert_pin_success: 1,
    cert_pin_net_request_error: 1,
    cert_pin_net_timeout: 1,
    cert_pin_abort: 1,
    cert_pin_missing_data: 0,
    cert_pin_failed: 0,
    cert_pin_invalid: 0,
    cert_pin_unknown_error: 0,
    content_sig_success: 0,
    content_sig_net_request_error: 0,
    content_sig_net_timeout: 0,
    content_sig_abort: 0,
    content_sig_missing_data: 0,
    content_sig_failed: 0,
    content_sig_invalid: 0,
    content_sig_unknown_error: 0,
  };
  checkGleanMetricCounts(expectedGleanValues);

  // Restore the URL override now that we're done.
  if (previousUrlOverride) {
    Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, previousUrlOverride);
  } else {
    Preferences.reset(GMPPrefs.KEY_URL_OVERRIDE);
  }
});

/**
 * Tests that installing found addons works as expected
 */
async function test_checkForAddons_installAddon(
  id,
  sizeConfig,
  defaultConfig,
  mirrorConfig,
  secondMirrorConfig,
  expectedError
) {
  info(
    "Running installAddon for id: " +
      id +
      ", sizeConfig: " +
      sizeConfig +
      ", defaultConfig: " +
      defaultConfig +
      ", mirrorConfig: " +
      mirrorConfig +
      ", secondMirrorConfig: " +
      secondMirrorConfig +
      ", expectedError: " +
      expectedError
  );

  let httpServer = new HttpServer();
  let dir = FileUtils.getDir("TmpD", []);
  httpServer.registerDirectory("/", dir);
  httpServer.start(-1);
  let testserverPort = httpServer.identity.primaryPort;
  let zipFileName = "test_" + id + "_GMP.zip";

  let zipURL = URL_HOST + ":" + testserverPort + "/" + zipFileName;
  info("zipURL: " + zipURL);

  let data = "e~=0.5772156649";
  let zipFile = createNewZipFile(zipFileName, data);
  let hashFunc = "sha256";
  let expectedDigest = await IOUtils.computeHexDigest(zipFile.path, hashFunc);
  let fileSize = zipFile.fileSize;
  if (sizeConfig === "mismatch") {
    fileSize = 1;
  }

  let badZipURL;
  let badZipFileName;
  let badZipFile;
  if (
    defaultConfig === "mismatch" ||
    mirrorConfig === "mismatch" ||
    secondMirrorConfig === "mismatch"
  ) {
    let badData = "e~=0.5772156648";
    badZipFileName = "test_" + id + "_bad_GMP.zip";
    badZipFile = createNewZipFile(badZipFileName, badData);
    badZipURL = URL_HOST + ":" + testserverPort + "/" + badZipFileName;
  }

  let missingZipURL = zipURL + ".missing";

  function selectUrl(config) {
    switch (config) {
      case "success":
        return zipURL;
      case "not_found":
        return missingZipURL;
      case "mismatch":
        return badZipURL;
      case "none":
        return null;
      default:
        throw new Error("bad config " + config);
    }
  }

  let defaultURL = selectUrl(defaultConfig);
  let mirrorURL = selectUrl(mirrorConfig);
  let secondMirrorURL = selectUrl(secondMirrorConfig);

  let responseXML =
    '<?xml version="1.0"?>' +
    "<updates>" +
    "    <addons>" +
    '        <addon id="' +
    id +
    '-gmp-gmpopenh264"' +
    (defaultURL ? ' URL="' + defaultURL + '"' : "") +
    '               hashFunction="' +
    hashFunc +
    '"' +
    '               hashValue="' +
    expectedDigest +
    '"' +
    (sizeConfig !== "none" ? ' size="' + fileSize + '"' : "") +
    '               version="1.1">' +
    (mirrorURL ? '          <mirror URL="' + mirrorURL + '"/>' : "") +
    (secondMirrorURL
      ? '          <mirror URL="' + secondMirrorURL + '"/>'
      : "") +
    "        </addon>" +
    "  </addons>" +
    "</updates>";

  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  Assert.equal(res.addons.length, 1);
  let gmpAddon = res.addons[0];
  Assert.ok(!gmpAddon.isInstalled);

  try {
    let extractedPaths = await installManager.installAddon(gmpAddon);
    if (sizeConfig === "mismatch") {
      Assert.ok(false); // installAddon() should have thrown.
    }
    Assert.equal(extractedPaths.length, 1);
    let extractedPath = extractedPaths[0];

    info("Extracted path: " + extractedPath);

    let extractedFile = Cc["@mozilla.org/file/local;1"].createInstance(
      Ci.nsIFile
    );

    extractedFile.initWithPath(extractedPath);
    Assert.ok(extractedFile.exists());
    let readData = readStringFromFile(extractedFile);
    Assert.equal(readData, data);

    // Make sure the prefs are set correctly
    Assert.ok(
      !!GMPPrefs.getInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, "", gmpAddon.id)
    );
    Assert.equal(
      GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_HASHVALUE, "", gmpAddon.id),
      expectedDigest
    );
    Assert.equal(
      GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION, "", gmpAddon.id),
      "1.1"
    );
    Assert.equal(
      GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_ABI, "", gmpAddon.id),
      UpdateUtils.ABI
    );
    // Make sure it reports as being installed
    Assert.ok(gmpAddon.isInstalled);

    // Cleanup
    extractedFile.parent.remove(true);
    httpServer.stop(function () {});
    installManager.uninit();
    Assert.equal(expectedError, null, "Succeeded without errors");
  } catch (ex) {
    Assert.ok(
      ex?.message?.match(expectedError),
      ex?.message + " matches " + expectedError
    );
  } finally {
    zipFile.remove(false);
    if (badZipFile) {
      badZipFile.remove(false);
    }
  }
}

add_task(
  async function test_checkForAddons_installAddon_includeSize_successURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_successURL_noMirror",
      /* sizeConfig */ "include",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_successURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_successURL_noMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_mismatchSize_successURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "mismatchSize_successURL_noMirror",
      /* sizeConfig */ "mismatch",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /Downloaded file was \d+ bytes but expected \d+ bytes/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_notFoundURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_notFoundURL_noMirror",
      /* sizeConfig */ "include",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /File download failed/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_notFoundURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_notFoundURL_noMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /File download failed/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_mismatchURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_mismatchURL_noMirror",
      /* sizeConfig */ "include",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /Hash was [\w`]+ but expected [\w`]+/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_mismatchURL_noMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_mismatchURL_noMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "none",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /Hash was [\w`]+ but expected [\w`]+/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_successURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_successURL_successMirror",
      /* sizeConfig */ "include",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_successURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_successURL_successMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_mismatchSize_successURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "mismatchSize_successURL_successMirror",
      /* sizeConfig */ "mismatch",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /Downloaded file was \d+ bytes but expected \d+ bytes/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_notFoundURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_notFoundURL_successMirror",
      /* sizeConfig */ "include",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_notFoundURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_notFoundURL_successMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_mismatchSize_notFoundURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "mismatchSize_notFoundURL_successMirror",
      /* sizeConfig */ "mismatch",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /File download failed/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_mismatchURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_mismatchURL_successMirror",
      /* sizeConfig */ "include",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_mismatchURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_mismatchURL_successMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_mismatchSize_mismatchURL_successMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "mismatchSize_mismatchURL_successMirror",
      /* sizeConfig */ "mismatch",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /Downloaded file was \d+ bytes but expected \d+ bytes/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_mismatchURL_mismatchMirror() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_mismatchURL_mismatchMirror",
      /* sizeConfig */ "none",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "mismatch",
      /* secondMirrorConfig */ "none",
      /* expectedError */ /Hash was [\w`]+ but expected [\w`]+/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_notFoundURL_notFoundMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_notFoundURL_notFoundMirrors",
      /* sizeConfig */ "none",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "not_found",
      /* secondMirrorConfig */ "not_found",
      /* expectedError */ /File download failed/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_notFoundURL_notFoundAndSuccessMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_notFoundURL_notFoundAndSuccessMirrors",
      /* sizeConfig */ "none",
      /* defaultConfig */ "not_found",
      /* mirrorConfig */ "not_found",
      /* secondMirrorConfig */ "success",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_mismatchURL_mismatchMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_mismatchURL_mismatchMirrors",
      /* sizeConfig */ "none",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "mismatch",
      /* secondMirrorConfig */ "mismatch",
      /* expectedError */ /Hash was [\w`]+ but expected [\w`]+/
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_noSize_mismatchURL_mismatchAndSuccessMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "noSize_mismatchURL_mismatchAndSuccessMirrors",
      /* sizeConfig */ "none",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "mismatch",
      /* secondMirrorConfig */ "success",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_successURL_notFoundMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_successURL_notFoundMirrors",
      /* sizeConfig */ "include",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "not_found",
      /* secondMirrorConfig */ "not_found",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_successURL_mismatchMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_successURL_mismatchMirrors",
      /* sizeConfig */ "include",
      /* defaultConfig */ "success",
      /* mirrorConfig */ "mismatch",
      /* secondMirrorConfig */ "mismatch",
      /* expectedError */ null
    );
  }
);

add_task(
  async function test_checkForAddons_installAddon_includeSize_mismatchURL_successAndMismatchMirrors() {
    await test_checkForAddons_installAddon(
      /* id */ "includeSize_mismatchURL_successAndMismatchMirrors",
      /* sizeConfig */ "include",
      /* defaultConfig */ "mismatch",
      /* mirrorConfig */ "success",
      /* secondMirrorConfig */ "mismatch",
      /* expectedError */ null
    );
  }
);

/**
 * Tests simpleCheckAndInstall when autoupdate is disabled for a GMP
 */
add_task(async function test_simpleCheckAndInstall_autoUpdateDisabled() {
  GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, false, OPEN_H264_ID);
  let responseXML =
    '<?xml version="1.0"?>' +
    "<updates>" +
    "    <addons>" +
    // valid openh264
    '        <addon id="gmp-gmpopenh264"' +
    '               URL="http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip"' +
    '               hashFunction="sha256"' +
    '               hashValue="1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee"' +
    '               version="1.1"/>' +
    "  </addons>" +
    "</updates>";

  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let result = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.simpleCheckAndInstall()
  );
  Assert.equal(result.status, "nothing-new-to-install");
  Preferences.reset(GMPPrefs.KEY_UPDATE_LAST_CHECK);
  GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, OPEN_H264_ID);
});

/**
 * Tests simpleCheckAndInstall nothing to install
 */
add_task(async function test_simpleCheckAndInstall_nothingToInstall() {
  let responseXML = '<?xml version="1.0"?><updates></updates>';

  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let result = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.simpleCheckAndInstall()
  );
  Assert.equal(result.status, "nothing-new-to-install");
});

/**
 * Tests simpleCheckAndInstall too frequent
 */
add_task(async function test_simpleCheckAndInstall_tooFrequent() {
  let responseXML = '<?xml version="1.0"?><updates></updates>';

  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let result = await ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.simpleCheckAndInstall()
  );
  Assert.equal(result.status, "too-frequent-no-check");
});

/**
 * Tests that installing addons when there is no server works as expected
 */
add_test(function test_installAddon_noServer() {
  let zipFileName = "test_GMP.zip";
  let zipURL = URL_HOST + ":0/" + zipFileName;

  let responseXML =
    '<?xml version="1.0"?>' +
    "<updates>" +
    "    <addons>" +
    '        <addon id="gmp-gmpopenh264"' +
    '               URL="' +
    zipURL +
    '"' +
    '               hashFunction="sha256"' +
    '               hashValue="11221cbda000347b054028b527a60e578f919cb10f322ef8077d3491c6fcb474"' +
    '               version="1.1"/>' +
    "  </addons>" +
    "</updates>";

  let myRequest = new mockRequest(200, responseXML);
  let installManager = new GMPInstallManager();
  let checkPromise = ProductAddonCheckerTestUtils.overrideServiceRequest(
    myRequest,
    () => installManager.checkForAddons()
  );
  checkPromise.then(
    res => {
      Assert.equal(res.addons.length, 1);
      let gmpAddon = res.addons[0];

      GMPInstallManager.overrideLeaveDownloadedZip = true;
      let installPromise = installManager.installAddon(gmpAddon);
      installPromise.then(
        () => {
          do_throw("No server for install should reject");
        },
        err => {
          Assert.ok(!!err);
          installManager.uninit();
          run_next_test();
        }
      );
    },
    () => {
      do_throw("check should not reject for install no server");
    }
  );
});

/***
 * Tests GMPExtractor (an internal component of GMPInstallManager) to ensure
 * it handles paths with certain characters.
 *
 * On Mac, test that the com.apple.quarantine extended attribute is removed
 * from installed plugin files.
 */

add_task(async function test_GMPExtractor_paths() {
  registerCleanupFunction(async function () {
    // Must stop holding on to the zip file using the JAR cache:
    let zipFile = new FileUtils.File(
      PathUtils.join(tempDir.path, "dummy_gmp.zip")
    );
    Services.obs.notifyObservers(zipFile, "flush-cache-entry");
    await IOUtils.remove(extractedDir, { recursive: true });
    await IOUtils.remove(tempDir.path, { recursive: true });
  });
  // Create a dir with the following in the name
  // - # -- this is used to delimit URI fragments and tests that
  //   we escape any internal URIs appropriately.
  // - 猫 -- ensure we handle non-ascii characters appropriately.
  const srcPath = PathUtils.join(
    Services.dirsvc.get("CurWorkD", Ci.nsIFile).path,
    "zips",
    "dummy_gmp.zip"
  );
  let tempDirName = "TmpDir#猫";
  let tempDir = FileUtils.getDir("TmpD", [tempDirName]);
  tempDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  let zipPath = PathUtils.join(tempDir.path, "dummy_gmp.zip");
  await IOUtils.copy(srcPath, zipPath);
  // The path inside the profile dir we'll extract to. Make sure we handle
  // the characters there too.
  let relativeExtractPath = "extracted#猫";
  let extractor = new GMPExtractor(zipPath, [relativeExtractPath]);
  let extractedPaths = await extractor.install();
  // extractedPaths should contain the files extracted. In this case we
  // should have a single file extracted to our profile dir -- the zip
  // contains two files, but one should be skipped by the extraction logic.
  Assert.equal(extractedPaths.length, 1, "One file should be extracted");
  Assert.ok(
    extractedPaths[0].includes("dummy_file.txt"),
    "dummy_file.txt should be on extracted path"
  );
  Assert.ok(
    !extractedPaths[0].includes("verified_contents.json"),
    "verified_contents.json should not be on extracted path"
  );
  let extractedDir = PathUtils.join(PathUtils.profileDir, relativeExtractPath);
  Assert.ok(
    await IOUtils.exists(extractedDir),
    "Extraction should have created a directory"
  );
  let extractedFile = PathUtils.join(
    PathUtils.profileDir,
    relativeExtractPath,
    "dummy_file.txt"
  );
  Assert.ok(
    await IOUtils.exists(extractedFile),
    "Extraction should have created dummy_file.txt"
  );
  if (AppConstants.platform == "macosx") {
    await Assert.rejects(
      IOUtils.getMacXAttr(extractedFile, "com.apple.quarantine"),
      /NotFoundError: Could not get extended attribute `com.apple.quarantine' from `.+': the file does not have the attribute/,
      "The 'com.apple.quarantine' attribute should not be present"
    );
  }
  let unextractedFile = PathUtils.join(
    PathUtils.profileDir,
    relativeExtractPath,
    "verified_contents.json"
  );
  Assert.ok(
    !(await IOUtils.exists(unextractedFile)),
    "Extraction should not have created verified_contents.json"
  );
});

/**
 * Returns the read stream into a string
 */
function readStringFromInputStream(inputStream) {
  let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  sis.init(inputStream);
  let text = sis.read(sis.available());
  sis.close();
  return text;
}

/**
 * Reads a string of text from a file.
 * This function only works with ASCII text.
 */
function readStringFromFile(file) {
  if (!file.exists()) {
    info("readStringFromFile - file doesn't exist: " + file.path);
    return null;
  }
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fis.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
  return readStringFromInputStream(fis);
}

/**
 * Constructs a mock xhr/ServiceRequest which is used for testing different
 * aspects of responses.
 */
function mockRequest(inputStatus, inputResponse, options) {
  this.inputStatus = inputStatus;
  this.inputResponse = inputResponse;
  this.status = 0;
  this.responseXML = null;
  this._aborted = false;
  this._onabort = null;
  this._onprogress = null;
  this._onerror = null;
  this._onload = null;
  this._onloadend = null;
  this._ontimeout = null;
  this._url = null;
  this._method = null;
  this._timeout = 0;
  this._notified = false;
  this._options = options || {};
}
mockRequest.prototype = {
  overrideMimeType() {},
  setRequestHeader() {},
  status: null,
  channel: { set notificationCallbacks(aVal) {} },
  open(aMethod, aUrl) {
    this.channel.originalURI = Services.io.newURI(aUrl);
    this._method = aMethod;
    this._url = aUrl;
  },
  abort() {
    this._dropRequest = true;
    this._notify(["abort", "loadend"]);
  },
  responseXML: null,
  responseText: null,
  send() {
    executeSoon(() => {
      try {
        if (this._options.dropRequest) {
          if (this._timeout > 0 && this._options.timeout) {
            this._notify(["timeout", "loadend"]);
          }
          return;
        }
        this.status = this.inputStatus;
        this.responseText = this.inputResponse;
        try {
          let parser = new DOMParser();
          this.responseXML = parser.parseFromString(
            this.inputResponse,
            "application/xml"
          );
        } catch (e) {
          this.responseXML = null;
        }
        if (this.inputStatus === 200) {
          this._notify(["load", "loadend"]);
        } else {
          this._notify(["error", "loadend"]);
        }
      } catch (ex) {
        do_throw(ex);
      }
    });
  },
  set onabort(aValue) {
    this._onabort = aValue;
  },
  get onabort() {
    return this._onabort;
  },
  set onprogress(aValue) {
    this._onprogress = aValue;
  },
  get onprogress() {
    return this._onprogress;
  },
  set onerror(aValue) {
    this._onerror = aValue;
  },
  get onerror() {
    return this._onerror;
  },
  set onload(aValue) {
    this._onload = aValue;
  },
  get onload() {
    return this._onload;
  },
  set onloadend(aValue) {
    this._onloadend = aValue;
  },
  get onloadend() {
    return this._onloadend;
  },
  set ontimeout(aValue) {
    this._ontimeout = aValue;
  },
  get ontimeout() {
    return this._ontimeout;
  },
  set timeout(aValue) {
    this._timeout = aValue;
  },
  _notify(events) {
    if (this._notified) {
      return;
    }
    this._notified = true;
    for (let item of events) {
      let k = "on" + item;
      if (this[k]) {
        info("Notifying " + item);
        let e = {
          target: this,
          type: item,
        };
        this[k](e);
      } else {
        info("Notifying " + item + ", but there are no listeners");
      }
    }
  },
  addEventListener(aEvent, aValue) {
    this[`_on${aEvent}`] = aValue;
  },
  get wrappedJSObject() {
    return this;
  },
};

/**
 * Creates a new zip file containing a file with the specified data
 * @param zipName The name of the zip file
 * @param data The data to go inside the zip for the filename entry1.info
 */
function createNewZipFile(zipName, data) {
  // Create a zip file which will be used for extracting
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.setData(data, data.length);
  let zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(
    Ci.nsIZipWriter
  );
  let zipFile = new FileUtils.File(PathUtils.join(PathUtils.tempDir, zipName));
  if (zipFile.exists()) {
    zipFile.remove(false);
  }
  // From prio.h
  const PR_RDWR = 0x04;
  const PR_CREATE_FILE = 0x08;
  const PR_TRUNCATE = 0x20;
  zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  zipWriter.addEntryStream(
    "entry1.info",
    Date.now() * PR_USEC_PER_MSEC,
    Ci.nsIZipWriter.COMPRESSION_BEST,
    stream,
    false
  );
  zipWriter.close();
  stream.close();
  info("zip file created on disk at: " + zipFile.path);
  return zipFile;
}

/***
 * Set up pref(s) as appropriate for content sig tests. Return the value of our
 * current GMP url override so it can be restored at test teardown.
 */

function setupContentSigTestPrefs() {
  Preferences.set("media.gmp-manager.checkContentSignature", true);

  // Return the URL override so tests can restore it to its previous value
  // once they're done.
  return Preferences.get(GMPPrefs.KEY_URL_OVERRIDE, "");
}

/***
 * Revert prefs used for content signature tests.
 *
 * @param previousUrlOverride - The GMP URL override value prior to test being
 * run. The function will revert the URL back to this, or reset the URL if no
 * value is passed.
 */
function revertContentSigTestPrefs(previousUrlOverride) {
  if (previousUrlOverride) {
    Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, previousUrlOverride);
  } else {
    Preferences.reset(GMPPrefs.KEY_URL_OVERRIDE);
  }
  Preferences.set("media.gmp-manager.checkContentSignature", false);
}

/***
 * Reset telemetry data related to gmp updates, and get the histogram
 * associated with MEDIA_GMP_UPDATE_XML_FETCH_RESULT.
 *
 * @returns The freshly cleared MEDIA_GMP_UPDATE_XML_FETCH_RESULT histogram.
 */
function resetGmpTelemetryAndGetHistogram() {
  Services.fog.testResetFOG();
  return TelemetryTestUtils.getAndClearHistogram(
    "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
  );
}

/***
 * A helper to check that glean metrics have expected counts.
 * @param expectedGleanValues a object that has properties with names set to glean metrics to be checked
 * and the values are the expected count. Eg { cert_pin_success: 1 }.
 */
function checkGleanMetricCounts(expectedGleanValues) {
  for (const property in expectedGleanValues) {
    if (Glean.gmp.updateXmlFetchResult[property].testGetValue()) {
      Assert.equal(
        Glean.gmp.updateXmlFetchResult[property].testGetValue(),
        expectedGleanValues[property],
        `${property} should have been recorded ${expectedGleanValues[property]} times`
      );
    } else {
      Assert.equal(
        expectedGleanValues[property],
        0,
        "testGetValue() being undefined should mean we expect a metric to not have been gathered"
      );
    }
  }
}

/***
 * Sets up a `HttpServer` for use in content singature checking tests. This
 * server will expose different endpoints that can be used to simulate different
 * pass and failure scenarios when fetching an update.xml file.
 *
 * @returns An object that has the following properties
 * - testServer - the HttpServer itself.
 * - promiseHolder - an object used to hold promises as properties. More complex test cases need this to sync different steps.
 * - validUpdateUri - a URI that should return a valid update xml + content sig.
 * - missingContentSigUri - a URI that returns a valid update xml, but misses the content sig header.
 * - badContentSigUri - a URI that returns a valid update xml, but provides data that is not a content sig in the header.
 * - invalidContentSigUri - a URI that returns a valid update xml, but provides an incorrect content sig.
 * - badXmlUri - a URI that returns an invalid update xml, but provides a correct content sig.
 * - x5uAbortUri - a URI that returns a valid update xml, but timesout the x5u request. Requires the caller to set
 *   `promiseHolder.installPromise` to the `checkForAddons()` promise` before hitting the endpoint. The server will set
 *   `promiseHolder.serverPromise` once it has started servicing the initial update request, and the caller should
 *   await that promise to ensure the server has restored state.
 * - x5uAbortUri - a URI that returns a valid update xml, but aborts the x5u request. Requires the caller to set
 *   `promiseHolder.installPromise` to the `checkForAddons()` promise` before hitting the endpoint. The server will set
 *   `promiseHolder.serverPromise` once it has started servicing the initial update request, and the caller should
 *   await that promise to ensure the server has restored state.
 */
function getTestServerForContentSignatureTests() {
  const testServer = new HttpServer();
  // Start the server so we can grab the identity. We need to know this so the
  // server can reference itself in the handlers that will be set up.
  testServer.start();
  const baseUri =
    testServer.identity.primaryScheme +
    "://" +
    testServer.identity.primaryHost +
    ":" +
    testServer.identity.primaryPort;

  // The promise holder has no properties by default. Different endpoints and
  // tests will set its properties as needed.
  let promiseHolder = {};

  const goodXml = readStringFromFile(do_get_file("good.xml"));
  // This sig is generated using the following command at mozilla-central root
  // `cat toolkit/mozapps/extensions/test/xpcshell/data/productaddons/good.xml | ./mach python security/manager/ssl/tests/unit/test_content_signing/pysign.py`
  // If test certificates are regenerated, this signature must also be.
  const goodXmlContentSignature =
    "7QYnPqFoOlS02BpDdIRIljzmPr6BFwPs1z1y8KJUBlnU7EVG6FbnXmVVt5Op9wDzHeN7pJOM7ANmTqU50IbHnV8q87wmY83QL4p6NZzjsFnWolFmwK2ZjlLnhyxFcVSz";

  // Setup endpoint to handle x5u lookups correctly.
  const validX5uPath = "/valid_x5u";
  const validCertChain = [
    readStringFromFile(do_get_file("content_signing_aus_ee.pem")),
    readStringFromFile(do_get_file("content_signing_int.pem")),
  ];
  testServer.registerPathHandler(validX5uPath, (req, res) => {
    res.write(validCertChain.join("\n"));
  });
  const validX5uUrl = baseUri + validX5uPath;

  // Handler for path that serves valid xml with valid signature.
  const validUpdatePath = "/valid_update.xml";
  testServer.registerPathHandler(validUpdatePath, (req, res) => {
    const validContentSignatureHeader = `x5u=${validX5uUrl}; p384ecdsa=${goodXmlContentSignature}`;
    res.setHeader("content-signature", validContentSignatureHeader);
    res.write(goodXml);
  });

  const missingContentSigPath = "/update_missing_content_sig.xml";
  testServer.registerPathHandler(missingContentSigPath, (req, res) => {
    // Content signature header omitted.
    res.write(goodXml);
  });

  const badContentSigPath = "/update_bad_content_sig.xml";
  testServer.registerPathHandler(badContentSigPath, (req, res) => {
    res.setHeader(
      "content-signature",
      `x5u=${validX5uUrl}; p384ecdsa=I'm a bad content signature`
    );
    res.write(goodXml);
  });

  // Make an invalid signature by change first char.
  const invalidXmlContentSignature = "Z" + goodXmlContentSignature.slice(1);
  const invalidContentSigPath = "/update_invalid_content_sig.xml";
  testServer.registerPathHandler(invalidContentSigPath, (req, res) => {
    res.setHeader(
      "content-signature",
      `x5u=${validX5uUrl}; p384ecdsa=${invalidXmlContentSignature}`
    );
    res.write(goodXml);
  });

  const badXml = readStringFromFile(do_get_file("bad.xml"));
  // This sig is generated using the following command at mozilla-central root
  // `cat toolkit/mozapps/extensions/test/xpcshell/data/productaddons/bad.xml | ./mach python security/manager/ssl/tests/unit/test_content_signing/pysign.py`
  // If test certificates are regenerated, this signature must also be.
  const badXmlContentSignature =
    "7QYnPqFoOlS02BpDdIRIljzmPr6BFwPs1z1y8KJUBlnU7EVG6FbnXmVVt5Op9wDz8YoQ_b-3i9rWpj40s8QZsMgo2eImx83LW9JE0d0z6sSAnwRb4lHFPpJXC_hv7wi7";
  const badXmlPath = "/bad.xml";
  testServer.registerPathHandler(badXmlPath, (req, res) => {
    const validContentSignatureHeader = `x5u=${validX5uUrl}; p384ecdsa=${badXmlContentSignature}`;
    res.setHeader("content-signature", validContentSignatureHeader);
    res.write(badXml);
  });

  const badX5uRequestPath = "/bad_x5u_request.xml";
  testServer.registerPathHandler(badX5uRequestPath, (req, res) => {
    const badX5uUrlHeader = `x5u=https://this.is.a/bad/url; p384ecdsa=${goodXmlContentSignature}`;
    res.setHeader("content-signature", badX5uUrlHeader);
    res.write(badXml);
  });

  const x5uTimeoutPath = "/x5u_timeout.xml";
  testServer.registerPathHandler(x5uTimeoutPath, (req, res) => {
    const validContentSignatureHeader = `x5u=${validX5uUrl}; p384ecdsa=${goodXmlContentSignature}`;
    // Write the correct header and xml, but setup the next request to timeout.
    // This should cause the request for the x5u URL to fail via timeout.
    let overriddenServiceRequest = new mockRequest(200, "", {
      dropRequest: true,
      timeout: true,
    });
    // We expose this promise so that tests can wait until the server has
    // reverted the overridden request (to avoid double overrides).
    promiseHolder.serverPromise =
      ProductAddonCheckerTestUtils.overrideServiceRequest(
        overriddenServiceRequest,
        () => {
          res.setHeader("content-signature", validContentSignatureHeader);
          res.write(goodXml);
          return promiseHolder.installPromise;
        }
      );
  });

  const x5uAbortPath = "/x5u_abort.xml";
  testServer.registerPathHandler(x5uAbortPath, (req, res) => {
    const validContentSignatureHeader = `x5u=${validX5uUrl}; p384ecdsa=${goodXmlContentSignature}`;
    // Write the correct header and xml, but setup the next request to fail.
    // This should cause the request for the x5u URL to fail via abort.
    let overriddenServiceRequest = new mockRequest(200, "", {
      dropRequest: true,
    });
    // We expose this promise so that tests can wait until the server has
    // reverted the overridden request (to avoid double overrides).
    promiseHolder.serverPromise =
      ProductAddonCheckerTestUtils.overrideServiceRequest(
        overriddenServiceRequest,
        () => {
          res.setHeader("content-signature", validContentSignatureHeader);
          res.write(goodXml);
          return promiseHolder.installPromise;
        }
      );
    setTimeout(() => {
      overriddenServiceRequest.abort();
    }, 100);
  });

  return {
    testServer,
    promiseHolder,
    validUpdateUri: baseUri + validUpdatePath,
    missingContentSigUri: baseUri + missingContentSigPath,
    badContentSigUri: baseUri + badContentSigPath,
    invalidContentSigUri: baseUri + invalidContentSigPath,
    badXmlUri: baseUri + badXmlPath,
    badX5uRequestUri: baseUri + badX5uRequestPath,
    x5uTimeoutUri: baseUri + x5uTimeoutPath,
    x5uAbortUri: baseUri + x5uAbortPath,
  };
}
