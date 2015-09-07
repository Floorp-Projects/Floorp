//
// This file tests the packaged app verifier - nsIPackagedAppVerifier
//
// ----------------------------------------------------------------------------
//
// All the test cases will ensure the callback order and args are exact the
// same as how and what we feed into the verifier. We also check if verifier
// gives the correct verification result like "is package signed", the
// "package origin", etc.
//
// Note that the actual signature verification is not done yet. If we claim a
// non-empty signature, the verifier will regard the verification as failed.
// The actual verification process is addressed by Bug 1178518. Non-developer mode
// test cases have to be modified once Bug 1178518 lands.
//
// We also test the developer mode here. In developer mode, no matter what kind
// of signautre do we initialize the verifier, the package is always said signed.
//

Cu.import("resource://gre/modules/Services.jsm");

////////////////////////////////////////////////////////////////
let gIoService = Cc["@mozilla.org/network/io-service;1"]
                   .getService(Ci.nsIIOService);

let gPrefs = Cc["@mozilla.org/preferences-service;1"]
               .getService(Components.interfaces.nsIPrefBranch);

let gVerifier = Cc["@mozilla.org/network/packaged-app-verifier;1"]
                  .createInstance(Ci.nsIPackagedAppVerifier);

let gCacheStorageService = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                             .getService(Ci.nsICacheStorageService);;

let gLoadContextInfoFactory =
  Cu.import("resource://gre/modules/LoadContextInfo.jsm", {}).LoadContextInfo;

const kUriIdx                 = 0;
const kStatusCodeIdx          = 1;
const kVerificationSuccessIdx = 2;

function enable_developer_mode()
{
  gPrefs.setBoolPref("network.http.packaged-apps-developer-mode", true);
}

function reset_developer_mode()
{
  gPrefs.clearUserPref("network.http.packaged-apps-developer-mode");
}

function createVerifierListener(aExpecetedCallbacks,
                                aExpectedOrigin,
                                aExpectedIsSigned,
                                aPackageCacheEntry) {
  let cnt = 0;

  return {
    onVerified: function(aIsManifest,
                         aUri,
                         aCacheEntry,
                         aStatusCode,
                         aIsLastPart,
                         aVerificationSuccess) {
      cnt++;

      let expectedCallback = aExpecetedCallbacks[cnt - 1];
      let isManifest = (cnt === 1);
      let isLastPart = (cnt === aExpecetedCallbacks.length);

      // Check if we are called back with correct info.
      equal(aIsManifest, isManifest, 'is manifest');
      equal(aUri.asciiSpec, expectedCallback[kUriIdx], 'URL');
      equal(aStatusCode, expectedCallback[kStatusCodeIdx], 'status code');
      equal(aIsLastPart, isLastPart, 'is lastPart');
      equal(aVerificationSuccess, expectedCallback[kVerificationSuccessIdx], 'verification result');

      if (isManifest) {
        // Check if the verifier got the right package info.
        equal(gVerifier.packageOrigin, aExpectedOrigin, 'package origin');
        equal(gVerifier.isPackageSigned, aExpectedIsSigned, 'is package signed');

        // Check if the verifier wrote the signed package origin to the cache.
        ok(!!aPackageCacheEntry, aPackageCacheEntry.key);
        let signePakOriginInCache = aPackageCacheEntry.getMetaDataElement('signed-pak-origin');
        equal(signePakOriginInCache,
              (aExpectedIsSigned ? aExpectedOrigin : ''),
              'signed-pak-origin in cache');
      }

      if (isLastPart) {
        reset_developer_mode();
        run_next_test();
      }
    },
  };
};

function feedResources(aExpectedCallbacks) {
  for (let i = 0; i < aExpectedCallbacks.length; i++) {
    let expectedCallback = aExpectedCallbacks[i];
    let isLastPart = (i === aExpectedCallbacks.length - 1);

    let uri = gIoService.newURI(expectedCallback[kUriIdx], null, null);
    gVerifier.onStartRequest(null, uri);

    let info = gVerifier.createResourceCacheInfo(uri,
                                                 null,
                                                 expectedCallback[kStatusCodeIdx],
                                                 isLastPart);

    gVerifier.onStopRequest(null, info, expectedCallback[kStatusCodeIdx]);
  }
}

function createPackageCache(aPackageUriAsAscii, aLoadContextInfo) {
  let cacheStorage =
      gCacheStorageService.memoryCacheStorage(aLoadContextInfo);

  let uri = gIoService.newURI(aPackageUriAsAscii, null, null);
  return cacheStorage.openTruncate(uri, '');
}

function test_no_signature(aDeveloperMode) {
  const kOrigin = 'http://foo.com';

  aDeveloperMode = !!aDeveloperMode;

  // If the package has no signature and not in developer mode, the package is unsigned
  // but the verification result is always true.

  const expectedCallbacks = [
  // URL                    statusCode   verificationResult
    [kOrigin + '/manifest', Cr.NS_OK,    true],
    [kOrigin + '/1.html',   Cr.NS_OK,    true],
    [kOrigin + '/2.js',     Cr.NS_OK,    true],
    [kOrigin + '/3.jpg',    Cr.NS_OK,    true],
    [kOrigin + '/4.html',   Cr.NS_OK,    true],
    [kOrigin + '/5.css',    Cr.NS_OK,    true],
  ];

  let isPackageSigned = false;

  // We only require the package URL to be different in each test case.
  let packageUriString = kOrigin + '/pak' + (aDeveloperMode ? '-dev' : '');

  let packageCacheEntry =
    createPackageCache(packageUriString, gLoadContextInfoFactory.default);

  let verifierListener = createVerifierListener(expectedCallbacks,
                                                kOrigin,
                                                isPackageSigned,
                                                packageCacheEntry);

  gVerifier.init(verifierListener, kOrigin, '', packageCacheEntry);

  feedResources(expectedCallbacks);
}

function test_invalid_signature(aDeveloperMode) {
  const kOrigin = 'http://bar.com';

  aDeveloperMode = !!aDeveloperMode;

  // Since we haven't implemented signature verification, the verification always
  // fails if the signature exists.

  let verificationResult = aDeveloperMode; // Verification always success in developer mode.
  let isPackageSigned = aDeveloperMode;   // Package is always considered as signed in developer mode.

  const expectedCallbacks = [
  // URL                      statusCode   verificationResult
    [kOrigin + '/manifest',   Cr.NS_OK,    verificationResult],
    [kOrigin + '/1.html',     Cr.NS_OK,    verificationResult],
    [kOrigin + '/2.js',       Cr.NS_OK,    verificationResult],
    [kOrigin + '/3.jpg',      Cr.NS_OK,    verificationResult],
    [kOrigin + '/4.html',     Cr.NS_OK,    verificationResult],
    [kOrigin + '/5.css',      Cr.NS_OK,    verificationResult],
  ];

  let packageUriString = kOrigin + '/pak' + (aDeveloperMode ? '-dev' : '');
  let packageCacheEntry =
    createPackageCache(packageUriString, gLoadContextInfoFactory.private);

  let verifierListener = createVerifierListener(expectedCallbacks,
                                                kOrigin,
                                                isPackageSigned,
                                                packageCacheEntry);

  gVerifier.init(verifierListener, kOrigin, 'invalid signature', packageCacheEntry);

  feedResources(expectedCallbacks);
}

function test_no_signature_developer_mode()
{
  enable_developer_mode()
  test_no_signature(true);
}

function test_invalid_signature_developer_mode()
{
  enable_developer_mode()
  test_invalid_signature(true);
}

function run_test()
{
  ok(!!gVerifier);

  // Test cases in non-developer mode.
  add_test(test_no_signature);
  add_test(test_invalid_signature);

  // Test cases in developer mode.
  add_test(test_no_signature_developer_mode);
  add_test(test_invalid_signature_developer_mode);

  // run tests
  run_next_test();
}
