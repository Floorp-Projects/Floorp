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
var gIoService = Cc["@mozilla.org/network/io-service;1"]
                   .getService(Ci.nsIIOService);

var gPrefs = Cc["@mozilla.org/preferences-service;1"]
               .getService(Components.interfaces.nsIPrefBranch);

var gVerifier = Cc["@mozilla.org/network/packaged-app-verifier;1"]
                  .createInstance(Ci.nsIPackagedAppVerifier);

var gCacheStorageService = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                             .getService(Ci.nsICacheStorageService);;

var gLoadContextInfoFactory =
  Cu.import("resource://gre/modules/LoadContextInfo.jsm", {}).LoadContextInfo;

const kUriIdx                 = 0;
const kStatusCodeIdx          = 1;
const kVerificationSuccessIdx = 2;
const kContentIdx             = 3;

function createVerifierListener(aExpecetedCallbacks,
                                aExpectedPackageId,
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
        equal(gVerifier.packageIdentifier, aExpectedPackageId, 'package identifier');
        equal(gVerifier.isPackageSigned, aExpectedIsSigned, 'is package signed');

        // Check if the verifier wrote the signed package origin to the cache.
        ok(!!aPackageCacheEntry, aPackageCacheEntry.key);
        let signePakIdInCache = aPackageCacheEntry.getMetaDataElement('package-id');
        equal(signePakIdInCache,
              (aExpectedIsSigned ? aExpectedPackageId : ''),
              'package-id in cache');
      }

      if (isLastPart) {
        run_next_test();
      }
    },
  };
};

function feedData(aString) {
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                           createInstance(Ci.nsIStringInputStream);
  stringStream.setData(aString, aString.length);
  gVerifier.onDataAvailable(null, null, stringStream, 0, aString.length);
}

function feedResources(aExpectedCallbacks, aSignature) {
  for (let i = 0; i < aExpectedCallbacks.length; i++) {
    let expectedCallback = aExpectedCallbacks[i];
    let isLastPart = (i === aExpectedCallbacks.length - 1);

    // Start request.
    let uri = gIoService.newURI(expectedCallback[kUriIdx], null, null);
    gVerifier.onStartRequest(null, uri);

    // Feed data at once.
    let contentString = expectedCallback[kContentIdx];
    if (contentString !== undefined) {
      feedData(contentString);
    }

    // Stop request.
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

function test_no_signature(aBypassVerification) {
  const kOrigin = 'http://foo.com';

  aBypassVerification = !!aBypassVerification;

  // If the package has no signature, the package is unsigned
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
  let packageUriString = kOrigin + '/pak' + (aBypassVerification ? '-dev' : '');

  let packageCacheEntry =
    createPackageCache(packageUriString, gLoadContextInfoFactory.default);

  let verifierListener = createVerifierListener(expectedCallbacks,
                                                '',
                                                isPackageSigned,
                                                packageCacheEntry);

  gVerifier.init(verifierListener, kOrigin, '', packageCacheEntry);

  feedResources(expectedCallbacks, '');
}

function test_invalid_signature(aBypassVerification) {
  const kOrigin = 'http://bar.com';

  aBypassVerification = !!aBypassVerification;

  // Since we haven't implemented signature verification, the verification always
  // fails if the signature exists.

  let verificationResult = aBypassVerification; // Verification always success in developer mode.
  let isPackageSigned = aBypassVerification;   // Package is always considered as signed in developer mode.

  const kPackagedId = '611FC2FE-491D-4A47-B3B3-43FBDF6F404F';
  const kManifestContent = 'Content-Location: manifest.webapp\r\n' +
                           'Content-Type: application/x-web-app-manifest+json\r\n' +
                           '\r\n' +
                           '{ "package-identifier": "' + kPackagedId + '",\n' +
                           '  "moz-package-origin": "' + kOrigin + '" }';

  const expectedCallbacks = [
  // URL                      statusCode   verificationResult     content
    [kOrigin + '/manifest',   Cr.NS_OK,    verificationResult,    kManifestContent],
    [kOrigin + '/1.html',     Cr.NS_OK,    verificationResult, 'abc'],
    [kOrigin + '/2.js',       Cr.NS_OK,    verificationResult, 'abc'],
    [kOrigin + '/3.jpg',      Cr.NS_OK,    verificationResult, 'abc'],
    [kOrigin + '/4.html',     Cr.NS_OK,    verificationResult, 'abc'],
    [kOrigin + '/5.css',      Cr.NS_OK,    verificationResult, 'abc'],
  ];

  let packageUriString = kOrigin + '/pak' + (aBypassVerification ? '-dev' : '');
  let packageCacheEntry =
    createPackageCache(packageUriString, gLoadContextInfoFactory.private);

  let verifierListener = createVerifierListener(expectedCallbacks,
                                                aBypassVerification ? kPackagedId : '',
                                                isPackageSigned,
                                                packageCacheEntry);

  let signature = 'manifest-signature: 11111111111111111111111';
  gVerifier.init(verifierListener, kOrigin, signature, packageCacheEntry);

  feedResources(expectedCallbacks, signature);
}

function test_invalid_signature_bypass_verification() {
  let pref = "network.http.signed-packages.trusted-origin";
  ok(!!Ci.nsISupportsString, "Ci.nsISupportsString");
  let origin = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
  origin.data = "http://bar.com";
  gPrefs.setComplexValue(pref, Ci.nsISupportsString, origin);
  test_invalid_signature(true);
  gPrefs.clearUserPref(pref);
}

function run_test()
{
  ok(!!gVerifier);

  add_test(test_no_signature);
  add_test(test_invalid_signature);
  add_test(test_invalid_signature_bypass_verification);

  // run tests
  run_next_test();
}
