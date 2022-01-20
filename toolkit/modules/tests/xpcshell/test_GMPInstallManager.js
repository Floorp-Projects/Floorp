/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

const URL_HOST = "http://localhost";
const PR_USEC_PER_MSEC = 1000;

const { GMPExtractor, GMPInstallManager } = ChromeUtils.import(
  "resource://gre/modules/GMPInstallManager.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { UpdateUtils } = ChromeUtils.import(
  "resource://gre/modules/UpdateUtils.jsm"
);
const { GMPPrefs, OPEN_H264_ID } = ChromeUtils.import(
  "resource://gre/modules/GMPUtils.jsm"
);
const { ProductAddonCheckerTestUtils } = ChromeUtils.import(
  "resource://gre/modules/addons/ProductAddonChecker.jsm"
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
    Assert.ok(res.usedFallback);
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
  Preferences.set("media.gmp-manager.checkContentSignature", true);

  // Store the old pref so we can restore it and avoid messing with other tests.
  let PREF_KEY_URL_OVERRIDE_BACKUP = Preferences.get(
    GMPPrefs.KEY_URL_OVERRIDE,
    ""
  );

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

  let goodXml = readStringFromFile(do_get_file("good.xml"));
  // This sig is generated using the following command at mozilla-central root
  // `cat toolkit/mozapps/extensions/test/xpcshell/data/productaddons/good.xml | ./mach python security/manager/ssl/tests/unit/test_content_signing/pysign.py`
  // If test certificates are regenerated, this signature must also be.
  const goodXmlContentSignature =
    "7QYnPqFoOlS02BpDdIRIljzmPr6BFwPs1z1y8KJUBlnU7EVG6FbnXmVVt5Op9wDzgvhXX7th8qFJvpPOZs_B_tHRDNJ8SK0HN95BAN15z3ZW2r95SSHmU-fP2JgoNOR3";

  // Setup endpoint to handle x5u lookups correctly.
  const validX5uPath = "/valid_x5u";
  const validCertChain = [
    readStringFromFile(do_get_file("content_signing_aus_ee.pem")),
    readStringFromFile(do_get_file("content_signing_int.pem")),
    readStringFromFile(do_get_file("content_signing_root.pem")),
  ];
  testServer.registerPathHandler(validX5uPath, (req, res) => {
    res.write(validCertChain.join("\n"));
  });
  const validX5uUrl = baseUri + validX5uPath;

  // Handler for path that serves valid xml with valid signature.
  const validContentSignatureHeader = `x5u=${validX5uUrl}; p384ecdsa=${goodXmlContentSignature}`;
  const updatePath = "/valid_update.xml";
  testServer.registerPathHandler(updatePath, (req, res) => {
    res.setHeader("content-signature", validContentSignatureHeader);
    res.write(goodXml);
  });

  // Override our root so that test cert chains will be valid.
  setCertRoot("content_signing_root.pem");

  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, baseUri + updatePath);

  const xmlFetchResultHistogram = TelemetryTestUtils.getAndClearHistogram(
    "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
  );

  let installManager = new GMPInstallManager();
  try {
    let res = await installManager.checkForAddons();
    Assert.ok(true, "checkForAddons should succeed");

    // Smoke test the results are as expected.
    // If the checkForAddons fails we'll get a fallback config,
    // so we'll get incorrect addons and these asserts will fail.
    Assert.equal(res.usedFallback, false);
    Assert.equal(res.addons.length, 5);
    Assert.equal(res.addons[0].id, "test1");
    Assert.equal(res.addons[1].id, "test2");
    Assert.equal(res.addons[2].id, "test3");
    Assert.equal(res.addons[3].id, "test4");
    Assert.equal(res.addons[4].id, undefined);
  } catch (e) {
    Assert.ok(false, "checkForAddons should succeed");
  }

  // # Ok content sig fetches should be 1, all others should be 0.
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 2, 1);

  // Tidy up afterwards.
  if (PREF_KEY_URL_OVERRIDE_BACKUP) {
    Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, PREF_KEY_URL_OVERRIDE_BACKUP);
  } else {
    Preferences.reset(GMPPrefs.KEY_URL_OVERRIDE);
  }
  Preferences.set("media.gmp-manager.checkContentSignature", false);
});

/**
 * Tests that checkForAddons() works as expected when content signature
 * checking is enabled and the signature check fails.
 */
add_task(async function test_checkForAddons_contentSignatureFailure() {
  Preferences.set("media.gmp-manager.checkContentSignature", true);

  // Store the old pref so we can restore it and avoid messing with other tests.
  let PREF_KEY_URL_OVERRIDE_BACKUP = Preferences.get(
    GMPPrefs.KEY_URL_OVERRIDE,
    ""
  );

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

  let goodXml = readStringFromFile(do_get_file("good.xml"));

  const updatePath = "/invalid_update.xml";
  testServer.registerPathHandler(updatePath, (req, res) => {
    // Content signature header omitted.
    res.write(goodXml);
  });

  Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, baseUri + updatePath);

  const xmlFetchResultHistogram = TelemetryTestUtils.getAndClearHistogram(
    "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
  );

  let installManager = new GMPInstallManager();
  try {
    let res = await installManager.checkForAddons();
    Assert.ok(true, "checkForAddons should succeed");

    // Smoke test the results are as expected.
    // Check addons will succeed above, but it will have fallen back to local
    // config. So the results will not be those from the HTTP server.
    Assert.equal(res.usedFallback, true);
    // Some platforms don't have fallback config for all GMPs, but we should
    // always get at least 1.
    Assert.greaterOrEqual(res.addons.length, 1);
    if (res.addons.length == 1) {
      Assert.equal(res.addons[0].id, "gmp-widevinecdm");
    } else {
      Assert.equal(res.addons[0].id, "gmp-gmpopenh264");
      Assert.equal(res.addons[1].id, "gmp-widevinecdm");
    }
  } catch (e) {
    Assert.ok(false, "checkForAddons should succeed");
  }

  // # Failed content sig fetches should be 1, all others should be 0.
  TelemetryTestUtils.assertHistogram(xmlFetchResultHistogram, 3, 1);

  // Tidy up afterwards.
  if (PREF_KEY_URL_OVERRIDE_BACKUP) {
    Preferences.set(GMPPrefs.KEY_URL_OVERRIDE, PREF_KEY_URL_OVERRIDE_BACKUP);
  } else {
    Preferences.reset(GMPPrefs.KEY_URL_OVERRIDE);
  }
  Preferences.set("media.gmp-manager.checkContentSignature", false);
});

/**
 * Tests that installing found addons works as expected
 */
async function test_checkForAddons_installAddon(
  id,
  includeSize,
  wantInstallReject
) {
  info(
    "Running installAddon for id: " +
      id +
      ", includeSize: " +
      includeSize +
      " and wantInstallReject: " +
      wantInstallReject
  );
  let httpServer = new HttpServer();
  let dir = FileUtils.getDir("TmpD", [], true);
  httpServer.registerDirectory("/", dir);
  httpServer.start(-1);
  let testserverPort = httpServer.identity.primaryPort;
  let zipFileName = "test_" + id + "_GMP.zip";

  let zipURL = URL_HOST + ":" + testserverPort + "/" + zipFileName;
  info("zipURL: " + zipURL);

  let data = "e~=0.5772156649";
  let zipFile = createNewZipFile(zipFileName, data);
  let hashFunc = "sha256";
  let expectedDigest = await ProductAddonCheckerTestUtils.computeHash(
    hashFunc,
    zipFile.path
  );
  let fileSize = zipFile.fileSize;
  if (wantInstallReject) {
    fileSize = 1;
  }

  let responseXML =
    '<?xml version="1.0"?>' +
    "<updates>" +
    "    <addons>" +
    '        <addon id="' +
    id +
    '-gmp-gmpopenh264"' +
    '               URL="' +
    zipURL +
    '"' +
    '               hashFunction="' +
    hashFunc +
    '"' +
    '               hashValue="' +
    expectedDigest +
    '"' +
    (includeSize ? ' size="' + fileSize + '"' : "") +
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
  Assert.ok(!gmpAddon.isInstalled);

  try {
    let extractedPaths = await installManager.installAddon(gmpAddon);
    if (wantInstallReject) {
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
    zipFile.remove(false);
    httpServer.stop(function() {});
    installManager.uninit();
  } catch (ex) {
    zipFile.remove(false);
    if (!wantInstallReject) {
      do_throw("install update should not reject " + ex.message);
    }
  }
}

add_task(test_checkForAddons_installAddon.bind(null, "1", true, false));
add_task(test_checkForAddons_installAddon.bind(null, "2", false, false));
add_task(test_checkForAddons_installAddon.bind(null, "3", true, true));

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
        extractedPaths => {
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
 */

add_task(async function test_GMPExtractor_paths() {
  registerCleanupFunction(async function() {
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
  let tempDir = FileUtils.getDir("TmpD", [tempDirName], true);
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
  let extractedDir = PathUtils.join(
    await PathUtils.getProfileDir(),
    relativeExtractPath
  );
  Assert.ok(
    await IOUtils.exists(extractedDir),
    "Extraction should have created a directory"
  );
  let extractedFile = PathUtils.join(
    await PathUtils.getProfileDir(),
    relativeExtractPath,
    "dummy_file.txt"
  );
  Assert.ok(
    await IOUtils.exists(extractedFile),
    "Extraction should have created dummy_file.txt"
  );
  let unextractedFile = PathUtils.join(
    await PathUtils.getProfileDir(),
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
 * Set the root certificate used by Firefox. Used to allow test certificate
 * chains to be valid.
 * @param {string} filename the name of the file containing the root cert.
 */
function setCertRoot(filename) {
  // Commonly certificates are represented as PEM. The format is roughly as
  // follows:
  //
  // -----BEGIN CERTIFICATE-----
  // [some lines of base64, each typically 64 characters long]
  // -----END CERTIFICATE-----
  //
  // However, nsIX509CertDB.constructX509FromBase64 and related functions do not
  // handle input of this form. Instead, they require a single string of base64
  // with no newlines or BEGIN/END headers. This is a helper function to convert
  // PEM to the format that nsIX509CertDB requires.
  function pemToBase64(pem) {
    return pem
      .replace(/-----BEGIN CERTIFICATE-----/, "")
      .replace(/-----END CERTIFICATE-----/, "")
      .replace(/[\r\n]/g, "");
  }

  let certBytes = readStringFromFile(do_get_file(filename));
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // Certs should always be in .pem format, don't bother with .dem handling.
  let cert = certdb.constructX509FromBase64(pemToBase64(certBytes));
  Services.prefs.setCharPref(
    "security.content.signature.root_hash",
    cert.sha256Fingerprint
  );
}

/**
 * Bare bones XMLHttpRequest implementation for testing onprogress, onerror,
 * and onload nsIDomEventListener handleEvent.
 */
function makeHandler(aVal) {
  if (typeof aVal == "function") {
    return { handleEvent: aVal };
  }
  return aVal;
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
  overrideMimeType(aMimetype) {},
  setRequestHeader(aHeader, aValue) {},
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
  send(aBody) {
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
    this._onabort = makeHandler(aValue);
  },
  get onabort() {
    return this._onabort;
  },
  set onprogress(aValue) {
    this._onprogress = makeHandler(aValue);
  },
  get onprogress() {
    return this._onprogress;
  },
  set onerror(aValue) {
    this._onerror = makeHandler(aValue);
  },
  get onerror() {
    return this._onerror;
  },
  set onload(aValue) {
    this._onload = makeHandler(aValue);
  },
  get onload() {
    return this._onload;
  },
  set onloadend(aValue) {
    this._onloadend = makeHandler(aValue);
  },
  get onloadend() {
    return this._onloadend;
  },
  set ontimeout(aValue) {
    this._ontimeout = makeHandler(aValue);
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
  addEventListener(aEvent, aValue, aCapturing) {
    // eslint-disable-next-line no-eval
    eval("this._on" + aEvent + " = aValue");
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
  let zipFile = FileUtils.getFile("TmpD", [zipName]);
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
