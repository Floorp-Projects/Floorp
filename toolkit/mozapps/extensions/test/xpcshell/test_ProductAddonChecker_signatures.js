"use strict";

const { ProductAddonChecker } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/ProductAddonChecker.sys.mjs"
);

Services.prefs.setBoolPref("media.gmp-manager.updateEnabled", true);

// Setup a test server for content signature tests.
const signedTestServer = new HttpServer();
const testDataDir = "data/productaddons/";

// Start the server so we can grab the identity. We need to know this so the
// server can reference itself in the handlers that will be set up.
signedTestServer.start();
const signedBaseUri =
  signedTestServer.identity.primaryScheme +
  "://" +
  signedTestServer.identity.primaryHost +
  ":" +
  signedTestServer.identity.primaryPort;

// Setup endpoint to handle x5u lookups correctly.
const validX5uPath = "/valid_x5u";
// These certificates are generated using ./mach generate-test-certs <path_to_certspec>
const validCertChain = loadCertChain(testDataDir + "content_signing", [
  "aus_ee",
  "int",
]);
signedTestServer.registerPathHandler(validX5uPath, (req, res) => {
  res.write(validCertChain.join("\n"));
});
const validX5uUrl = signedBaseUri + validX5uPath;

// Setup endpoint to handle x5u lookups incorrectly.
const invalidX5uPath = "/invalid_x5u";
const invalidCertChain = loadCertChain(testDataDir + "content_signing", [
  "aus_ee",
  // This cert chain is missing the intermediate cert!
]);
signedTestServer.registerPathHandler(invalidX5uPath, (req, res) => {
  res.write(invalidCertChain.join("\n"));
});
const invalidX5uUrl = signedBaseUri + invalidX5uPath;

// Will hold the XML data from good.xml.
let goodXml;
// This sig is generated using the following command at mozilla-central root
// `cat toolkit/mozapps/extensions/test/xpcshell/data/productaddons/good.xml | ./mach python security/manager/ssl/tests/unit/test_content_signing/pysign.py`
// If test certificates are regenerated, this signature must also be.
const goodXmlContentSignature =
  "7QYnPqFoOlS02BpDdIRIljzmPr6BFwPs1z1y8KJUBlnU7EVG6FbnXmVVt5Op9wDzHeN7pJOM7ANmTqU50IbHnV8q87wmY83QL4p6NZzjsFnWolFmwK2ZjlLnhyxFcVSz";

const goodXmlPath = "/good.xml";
// Requests use query strings to test different signature states.
const validSignatureQuery = "validSignature";
const invalidSignatureQuery = "invalidSignature";
const missingSignatureQuery = "missingSignature";
const incompleteSignatureQuery = "incompleteSignature";
const badX5uSignatureQuery = "badX5uSignature";
signedTestServer.registerPathHandler(goodXmlPath, (req, res) => {
  if (req.queryString == validSignatureQuery) {
    res.setHeader(
      "content-signature",
      `x5u=${validX5uUrl}; p384ecdsa=${goodXmlContentSignature}`
    );
  } else if (req.queryString == invalidSignatureQuery) {
    res.setHeader("content-signature", `x5u=${validX5uUrl}; p384ecdsa=garbage`);
  } else if (req.queryString == missingSignatureQuery) {
    // Intentionally don't set the header.
  } else if (req.queryString == incompleteSignatureQuery) {
    res.setHeader(
      "content-signature",
      `x5u=${validX5uUrl}` // There's no p384ecdsa part!
    );
  } else if (req.queryString == badX5uSignatureQuery) {
    res.setHeader(
      "content-signature",
      `x5u=${invalidX5uUrl}; p384ecdsa=${goodXmlContentSignature}`
    );
  } else {
    Assert.ok(
      false,
      "Invalid queryString passed to server! Tests shouldn't do that!"
    );
  }
  res.write(goodXml);
});

// Handle aysnc load of good.xml.
add_task(async function load_good_xml() {
  goodXml = await IOUtils.readUTF8(do_get_file(testDataDir + "good.xml").path);
});

add_task(async function test_valid_content_signature() {
  try {
    const res = await ProductAddonChecker.getProductAddonList(
      signedBaseUri + goodXmlPath + "?" + validSignatureQuery,
      /*allowNonBuiltIn*/ false,
      /*allowedCerts*/ false,
      /*verifyContentSignature*/ true,
      /*trustedContentSignatureRoot*/ Ci.nsIX509CertDB.AppXPCShellRoot
    );
    Assert.ok(true, "Should successfully get addon list");

    // Smoke test the results are as expected.
    Assert.equal(res.addons[0].id, "test1");
    Assert.equal(res.addons[1].id, "test2");
    Assert.equal(res.addons[2].id, "test3");
    Assert.equal(res.addons[3].id, "test4");
    Assert.equal(res.addons[4].id, undefined);
  } catch (e) {
    Assert.ok(
      false,
      `Should successfully get addon list, instead failed with ${e}`
    );
  }
});

add_task(async function test_invalid_content_signature() {
  try {
    await ProductAddonChecker.getProductAddonList(
      signedBaseUri + goodXmlPath + "?" + invalidSignatureQuery,
      /*allowNonBuiltIn*/ false,
      /*allowedCerts*/ false,
      /*verifyContentSignature*/ true,
      /*trustedContentSignatureRoot*/ Ci.nsIX509CertDB.AppXPCShellRoot
    );
    Assert.ok(false, "Should fail to get addon list");
  } catch (e) {
    Assert.ok(true, "Should fail to get addon list");
    // The nsIContentSignatureVerifier will throw an error on this path,
    // check that we've caught and re-thrown, but don't check the full error
    // message as it's messy and subject to change.
    Assert.ok(
      e.message.startsWith("Content signature validation failed:"),
      "Should get signature failure message"
    );
  }
});

add_task(async function test_missing_content_signature_header() {
  try {
    await ProductAddonChecker.getProductAddonList(
      signedBaseUri + goodXmlPath + "?" + missingSignatureQuery,
      /*allowNonBuiltIn*/ false,
      /*allowedCerts*/ false,
      /*verifyContentSignature*/ true,
      /*trustedContentSignatureRoot*/ Ci.nsIX509CertDB.AppXPCShellRoot
    );
    Assert.ok(false, "Should fail to get addon list");
  } catch (e) {
    Assert.ok(true, "Should fail to get addon list");
    Assert.equal(
      e.addonCheckerErr,
      ProductAddonChecker.VERIFICATION_MISSING_DATA_ERR
    );
    Assert.equal(
      e.message,
      "Content signature validation failed: missing content signature header"
    );
  }
});

add_task(async function test_incomplete_content_signature_header() {
  try {
    await ProductAddonChecker.getProductAddonList(
      signedBaseUri + goodXmlPath + "?" + incompleteSignatureQuery,
      /*allowNonBuiltIn*/ false,
      /*allowedCerts*/ false,
      /*verifyContentSignature*/ true,
      /*trustedContentSignatureRoot*/ Ci.nsIX509CertDB.AppXPCShellRoot
    );
    Assert.ok(false, "Should fail to get addon list");
  } catch (e) {
    Assert.ok(true, "Should fail to get addon list");
    Assert.equal(
      e.addonCheckerErr,
      ProductAddonChecker.VERIFICATION_MISSING_DATA_ERR
    );
    Assert.equal(
      e.message,
      "Content signature validation failed: missing signature"
    );
  }
});

add_task(async function test_bad_x5u_content_signature_header() {
  try {
    await ProductAddonChecker.getProductAddonList(
      signedBaseUri + goodXmlPath + "?" + badX5uSignatureQuery,
      /*allowNonBuiltIn*/ false,
      /*allowedCerts*/ false,
      /*verifyContentSignature*/ true,
      /*trustedContentSignatureRoot*/ Ci.nsIX509CertDB.AppXPCShellRoot
    );
    Assert.ok(false, "Should fail to get addon list");
  } catch (e) {
    Assert.ok(true, "Should fail to get addon list");
    Assert.equal(
      e.addonCheckerErr,
      ProductAddonChecker.VERIFICATION_INVALID_ERR
    );
    Assert.equal(e.message, "Content signature is not valid");
  }
});
