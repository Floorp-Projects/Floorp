/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

var Cm = Components.manager;
const URL_HOST = "http://localhost";

var GMPScope = ChromeUtils.import("resource://gre/modules/GMPInstallManager.jsm", {});
var GMPInstallManager = GMPScope.GMPInstallManager;

ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm");

var ProductAddonCheckerScope = ChromeUtils.import("resource://gre/modules/addons/ProductAddonChecker.jsm", {});

do_get_profile();

function run_test() {
 ChromeUtils.import("resource://gre/modules/Preferences.jsm");
  Preferences.set("media.gmp.log.dump", true);
  Preferences.set("media.gmp.log.level", 0);
  run_next_test();
}

/**
 * Tests that the helper used for preferences works correctly
 */
add_task(async function test_prefs() {
  let addon1 = "addon1", addon2 = "addon2";

  GMPScope.GMPPrefs.setString(GMPScope.GMPPrefs.KEY_URL, "http://not-really-used");
  GMPScope.GMPPrefs.setString(GMPScope.GMPPrefs.KEY_URL_OVERRIDE, "http://not-really-used-2");
  GMPScope.GMPPrefs.setInt(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, 1, addon1);
  GMPScope.GMPPrefs.setString(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, "2", addon1);
  GMPScope.GMPPrefs.setInt(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, 3, addon2);
  GMPScope.GMPPrefs.setInt(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, 4, addon2);
  GMPScope.GMPPrefs.setBool(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, false, addon2);
  GMPScope.GMPPrefs.setBool(GMPScope.GMPPrefs.KEY_CERT_CHECKATTRS, true);

  Assert.equal(GMPScope.GMPPrefs.getString(GMPScope.GMPPrefs.KEY_URL), "http://not-really-used");
  Assert.equal(GMPScope.GMPPrefs.getString(GMPScope.GMPPrefs.KEY_URL_OVERRIDE),
               "http://not-really-used-2");
  Assert.equal(GMPScope.GMPPrefs.getInt(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, "", addon1), 1);
  Assert.equal(GMPScope.GMPPrefs.getString(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, "", addon1), "2");
  Assert.equal(GMPScope.GMPPrefs.getInt(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, "", addon2), 3);
  Assert.equal(GMPScope.GMPPrefs.getInt(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, "", addon2), 4);
  Assert.equal(GMPScope.GMPPrefs.getBool(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, undefined, addon2),
               false);
  Assert.ok(GMPScope.GMPPrefs.getBool(GMPScope.GMPPrefs.KEY_CERT_CHECKATTRS));
  GMPScope.GMPPrefs.setBool(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, addon2);
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
  overrideXHR(200, "");
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();
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
  overrideXHR(200, "");
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();
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
  overrideXHR(200, "<updates></updates>");
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 0);
  installManager.uninit();
});

/**
 * Tests that empty addons element returned resolves with no addons
 */
add_task(async function test_checkForAddons_emptyAddonsElement() {
  overrideXHR(200, "<updates><addons/></updates>");
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 0);
  installManager.uninit();
});

/**
 * Tests that a response with the wrong root element rejects
 */
add_test(function test_checkForAddons_wrongResponseXML() {
  overrideXHR(200, "<digits_of_pi>3.141592653589793....</digits_of_pi>");
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();
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
  overrideXHR(404, "");
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();
  promise.then(res => {
    Assert.ok(res.usedFallback);
    installManager.uninit();
    run_next_test();
  });
});

/**
 * Tests that a xhr abort() works as expected
 */
add_test(function test_checkForAddons_abort() {
  let overriddenXhr = overrideXHR(200, "", { dropRequest: true} );
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();

  // Since the XHR is created in checkForAddons asynchronously,
  // we need to delay aborting till the XHR is running.
  // Since checkForAddons returns a Promise already only after
  // the abort is triggered, we can't use that, and instead
  // we'll use a fake timer.
  setTimeout(() => {
    overriddenXhr.abort();
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
  overrideXHR(200, "", { dropRequest: true, timeout: true });
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();
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
  let PREF_KEY_URL_OVERRIDE_BACKUP =
    Preferences.get(GMPScope.GMPPrefs.KEY_URL_OVERRIDE, "");
  Preferences.reset(GMPScope.GMPPrefs.KEY_URL_OVERRIDE);

  let CERTS_BRANCH_DOT_ONE = GMPScope.GMPPrefs.KEY_CERTS_BRANCH + ".1";
  let PREF_CERTS_BRANCH_DOT_ONE_BACKUP =
    Preferences.get(CERTS_BRANCH_DOT_ONE, "");
  Services.prefs.setCharPref(CERTS_BRANCH_DOT_ONE, "funky value");


  overrideXHR(200, "");
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();
  promise.then(res => {
    Assert.ok(res.usedFallback);
    installManager.uninit();
    if (PREF_KEY_URL_OVERRIDE_BACKUP) {
      Preferences.set(GMPScope.GMPPrefs.KEY_URL_OVERRIDE,
        PREF_KEY_URL_OVERRIDE_BACKUP);
    }
    if (PREF_CERTS_BRANCH_DOT_ONE_BACKUP) {
      Preferences.set(CERTS_BRANCH_DOT_ONE,
        PREF_CERTS_BRANCH_DOT_ONE_BACKUP);
    }
    run_next_test();
  });
});

/**
 * Tests that gettinga a funky non XML response works as expected
 */
add_test(function test_checkForAddons_notXML() {
  overrideXHR(200, "3.141592653589793....");
  let installManager = new GMPInstallManager();
  let promise = installManager.checkForAddons();

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
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "    <addons>" +
    "        <addon id=\"gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha256\"" +
    "               hashValue=\"1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"1.1\"/>" +
    "  </addons>" +
    "</updates>";
  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 1);
  let gmpAddon = res.gmpAddons[0];
  Assert.equal(gmpAddon.id, "gmp-gmpopenh264");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(gmpAddon.hashValue, "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee");
  Assert.equal(gmpAddon.version, "1.1");
  Assert.equal(gmpAddon.size, undefined);
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
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "    <addons>" +
    "        <addon id=\"openh264-plugin-no-at-symbol\"" +
    "               URL=\"http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha256\"" +
    "               size=\"42\"" +
    "               hashValue=\"1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"1.1\"/>" +
    "  </addons>" +
    "</updates>";
  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 1);
  let gmpAddon = res.gmpAddons[0];
  Assert.equal(gmpAddon.id, "openh264-plugin-no-at-symbol");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(gmpAddon.hashValue, "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee");
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
add_task(async function test_checkForAddons_multipleAddonNoUpdatesSomeInvalid() {
  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "    <addons>" +
    // valid openh264
    "        <addon id=\"gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha256\"" +
    "               hashValue=\"1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"1.1\"/>" +
    // valid not openh264
    "        <addon id=\"NOT-gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha512\"" +
    "               hashValue=\"141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"9.1\"/>" +
    // noid
    "        <addon notid=\"NOT-gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha512\"" +
    "               hashValue=\"141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"9.1\"/>" +
    // no URL
    "        <addon id=\"NOT-gmp-gmpopenh264\"" +
    "               notURL=\"http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha512\"" +
    "               hashValue=\"141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"9.1\"/>" +
    // no hash function
    "        <addon id=\"NOT-gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip\"" +
    "               nothashFunction=\"sha512\"" +
    "               hashValue=\"141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"9.1\"/>" +
    // no hash function
    "        <addon id=\"NOT-gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha512\"" +
    "               nothashValue=\"141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"9.1\"/>" +
    // not version
    "        <addon id=\"NOT-gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha512\"" +
    "               hashValue=\"141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               notversion=\"9.1\"/>" +
    "  </addons>" +
    "</updates>";
  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 7);
  let gmpAddon = res.gmpAddons[0];
  Assert.equal(gmpAddon.id, "gmp-gmpopenh264");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(gmpAddon.hashValue, "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee");
  Assert.equal(gmpAddon.version, "1.1");
  Assert.ok(gmpAddon.isValid);
  Assert.ok(!gmpAddon.isInstalled);

  gmpAddon = res.gmpAddons[1];
  Assert.equal(gmpAddon.id, "NOT-gmp-gmpopenh264");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/NOT-gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha512");
  Assert.equal(gmpAddon.hashValue, "141592656f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee");
  Assert.equal(gmpAddon.version, "9.1");
  Assert.ok(gmpAddon.isValid);
  Assert.ok(!gmpAddon.isInstalled);

  for (let i = 2; i < res.gmpAddons.length; i++) {
    Assert.ok(!res.gmpAddons[i].isValid);
    Assert.ok(!res.gmpAddons[i].isInstalled);
  }
  installManager.uninit();
});

/**
 * Tests that checking for addons when there are also updates available
 * works as expected.
 */
add_task(async function test_checkForAddons_updatesWithAddons() {
  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "    <updates>" +
    "        <update type=\"minor\" displayVersion=\"33.0a1\" appVersion=\"33.0a1\" platformVersion=\"33.0a1\" buildID=\"20140628030201\">" +
    "        <patch type=\"complete\" URL=\"http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/2014/06/2014-06-28-03-02-01-mozilla-central/firefox-33.0a1.en-US.mac.complete.mar\" hashFunction=\"sha512\" hashValue=\"f3f90d71dff03ae81def80e64bba3e4569da99c9e15269f731c2b167c4fc30b3aed9f5fee81c19614120230ca333e73a5e7def1b8e45d03135b2069c26736219\" size=\"85249896\"/>" +
    "    </update>" +
    "    <addons>" +
    "        <addon id=\"gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha256\"" +
    "               hashValue=\"1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"1.1\"/>" +
    "  </addons>" +
    "</updates>";
  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 1);
  let gmpAddon = res.gmpAddons[0];
  Assert.equal(gmpAddon.id, "gmp-gmpopenh264");
  Assert.equal(gmpAddon.URL, "http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip");
  Assert.equal(gmpAddon.hashFunction, "sha256");
  Assert.equal(gmpAddon.hashValue, "1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee");
  Assert.equal(gmpAddon.version, "1.1");
  Assert.ok(gmpAddon.isValid);
  Assert.ok(!gmpAddon.isInstalled);
  installManager.uninit();
});

/**
 * Tests that installing found addons works as expected
 */
async function test_checkForAddons_installAddon(id, includeSize, wantInstallReject) {
  info("Running installAddon for id: " + id +
       ", includeSize: " + includeSize +
       " and wantInstallReject: " + wantInstallReject);
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
  let expectedDigest = await ProductAddonCheckerScope.computeHash(hashFunc, zipFile.path);
  let fileSize = zipFile.fileSize;
  if (wantInstallReject) {
    fileSize = 1;
  }

  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "    <addons>" +
    "        <addon id=\"" + id + "-gmp-gmpopenh264\"" +
    "               URL=\"" + zipURL + "\"" +
    "               hashFunction=\"" + hashFunc + "\"" +
    "               hashValue=\"" + expectedDigest + "\"" +
    (includeSize ? " size=\"" + fileSize + "\"" : "") +
    "               version=\"1.1\"/>" +
    "  </addons>" +
    "</updates>";

  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let res = await installManager.checkForAddons();
  Assert.equal(res.gmpAddons.length, 1);
  let gmpAddon = res.gmpAddons[0];
  Assert.ok(!gmpAddon.isInstalled);

  try {
    let extractedPaths = await installManager.installAddon(gmpAddon);
    if (wantInstallReject) {
      Assert.ok(false); // installAddon() should have thrown.
    }
    Assert.equal(extractedPaths.length, 1);
    let extractedPath = extractedPaths[0];

    info("Extracted path: " + extractedPath);

    let extractedFile = Cc["@mozilla.org/file/local;1"].
                        createInstance(Ci.nsIFile);
    extractedFile.initWithPath(extractedPath);
    Assert.ok(extractedFile.exists());
    let readData = readStringFromFile(extractedFile);
    Assert.equal(readData, data);

    // Make sure the prefs are set correctly
    Assert.ok(!!GMPScope.GMPPrefs.getInt(
      GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, "", gmpAddon.id));
    Assert.equal(GMPScope.GMPPrefs.getString(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, "",
                                             gmpAddon.id),
                 "1.1");
    Assert.equal(GMPScope.GMPPrefs.getString(GMPScope.GMPPrefs.KEY_PLUGIN_ABI, "",
                                             gmpAddon.id),
                 UpdateUtils.ABI);
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
  GMPScope.GMPPrefs.setBool(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, false, GMPScope.OPEN_H264_ID);
  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "    <addons>" +
    // valid openh264
    "        <addon id=\"gmp-gmpopenh264\"" +
    "               URL=\"http://127.0.0.1:8011/gmp-gmpopenh264-1.1.zip\"" +
    "               hashFunction=\"sha256\"" +
    "               hashValue=\"1118b90d6f645eefc2b99af17bae396636ace1e33d079c88de715177584e2aee\"" +
    "               version=\"1.1\"/>" +
    "  </addons>" +
    "</updates>";

  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let result = await installManager.simpleCheckAndInstall();
  Assert.equal(result.status, "nothing-new-to-install");
  Preferences.reset(GMPScope.GMPPrefs.KEY_UPDATE_LAST_CHECK);
  GMPScope.GMPPrefs.setBool(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, GMPScope.OPEN_H264_ID);
});

/**
 * Tests simpleCheckAndInstall nothing to install
 */
add_task(async function test_simpleCheckAndInstall_nothingToInstall() {
  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "</updates>";

  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let result = await installManager.simpleCheckAndInstall();
  Assert.equal(result.status, "nothing-new-to-install");
});

/**
 * Tests simpleCheckAndInstall too frequent
 */
add_task(async function test_simpleCheckAndInstall_tooFrequent() {
  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "</updates>";

  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let result = await installManager.simpleCheckAndInstall();
  Assert.equal(result.status, "too-frequent-no-check");
});

/**
 * Tests that installing addons when there is no server works as expected
 */
add_test(function test_installAddon_noServer() {
  let zipFileName = "test_GMP.zip";
  let zipURL = URL_HOST + ":0/" + zipFileName;

  let responseXML =
    "<?xml version=\"1.0\"?>" +
    "<updates>" +
    "    <addons>" +
    "        <addon id=\"gmp-gmpopenh264\"" +
    "               URL=\"" + zipURL + "\"" +
    "               hashFunction=\"sha256\"" +
    "               hashValue=\"11221cbda000347b054028b527a60e578f919cb10f322ef8077d3491c6fcb474\"" +
    "               version=\"1.1\"/>" +
    "  </addons>" +
    "</updates>";

  overrideXHR(200, responseXML);
  let installManager = new GMPInstallManager();
  let checkPromise = installManager.checkForAddons();
  checkPromise.then(res => {
    Assert.equal(res.gmpAddons.length, 1);
    let gmpAddon = res.gmpAddons[0];

    GMPInstallManager.overrideLeaveDownloadedZip = true;
    let installPromise = installManager.installAddon(gmpAddon);
    installPromise.then(extractedPaths => {
      do_throw("No server for install should reject");
    }, err => {
      Assert.ok(!!err);
      installManager.uninit();
      run_next_test();
    });
  }, () => {
    do_throw("check should not reject for install no server");
  });
});

/**
 * Returns the read stream into a string
 */
function readStringFromInputStream(inputStream) {
  let sis = Cc["@mozilla.org/scriptableinputstream;1"].
            createInstance(Ci.nsIScriptableInputStream);
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
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  fis.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
  return readStringFromInputStream(fis);
}

/**
 * Bare bones XMLHttpRequest implementation for testing onprogress, onerror,
 * and onload nsIDomEventListener handleEvent.
 */
function makeHandler(aVal) {
  if (typeof aVal == "function")
    return { handleEvent: aVal };
  return aVal;
}
/**
 * Constructs a mock xhr which is used for testing different aspects
 * of responses.
 */
function xhr(inputStatus, inputResponse, options) {
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
xhr.prototype = {
  overrideMimeType(aMimetype) { },
  setRequestHeader(aHeader, aValue) { },
  status: null,
  channel: { set notificationCallbacks(aVal) { } },
  open(aMethod, aUrl) {
    this.channel.originalURI = Services.io.newURI(aUrl);
    this._method = aMethod; this._url = aUrl;
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
          let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                createInstance(Ci.nsIDOMParser);
          this.responseXML = parser.parseFromString(this.inputResponse,
            "application/xml");
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
  set onabort(aValue) { this._onabort = makeHandler(aValue); },
  get onabort() { return this._onabort; },
  set onprogress(aValue) { this._onprogress = makeHandler(aValue); },
  get onprogress() { return this._onprogress; },
  set onerror(aValue) { this._onerror = makeHandler(aValue); },
  get onerror() { return this._onerror; },
  set onload(aValue) { this._onload = makeHandler(aValue); },
  get onload() { return this._onload; },
  set onloadend(aValue) { this._onloadend = makeHandler(aValue); },
  get onloadend() { return this._onloadend; },
  set ontimeout(aValue) { this._ontimeout = makeHandler(aValue); },
  get ontimeout() { return this._ontimeout; },
  set timeout(aValue) { this._timeout = aValue; },
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
  get wrappedJSObject() { return this; }
};

/**
 * Helper used to overrideXHR requests (no matter to what URL) with the
 * specified status and response.
 * @param status The status you want to get back when an XHR request is made
 * @param response The response you want to get back when an XHR request is made
 */
function overrideXHR(status, response, options) {
  overrideXHR.myxhr = new xhr(status, response, options);
  ProductAddonCheckerScope.CreateXHR = function() {
    return overrideXHR.myxhr;
  };
  return overrideXHR.myxhr;
}

/**
 * Creates a new zip file containing a file with the specified data
 * @param zipName The name of the zip file
 * @param data The data to go inside the zip for the filename entry1.info
 */
function createNewZipFile(zipName, data) {
   // Create a zip file which will be used for extracting
    let stream = Cc["@mozilla.org/io/string-input-stream;1"].
                 createInstance(Ci.nsIStringInputStream);
    stream.setData(data, data.length);
    let zipWriter = Cc["@mozilla.org/zipwriter;1"].
                    createInstance(Components.interfaces.nsIZipWriter);
    let zipFile = FileUtils.getFile("TmpD", [zipName]);
    if (zipFile.exists()) {
      zipFile.remove(false);
    }
    // From prio.h
    const PR_RDWR = 0x04;
    const PR_CREATE_FILE = 0x08;
    const PR_TRUNCATE    = 0x20;
    zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
    zipWriter.addEntryStream("entry1.info", Date.now(),
                             Ci.nsIZipWriter.COMPRESSION_BEST, stream, false);
    zipWriter.close();
    stream.close();
    info("zip file created on disk at: " + zipFile.path);
    return zipFile;
}
