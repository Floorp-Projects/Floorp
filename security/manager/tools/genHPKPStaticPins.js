/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell \
//                                  [path to]/genHPKPStaticpins.js \
//                                  [absolute path to]/PreloadedHPKPins.json \
//                                  [absolute path to]/default-ee.der \
//                                  [absolute path to]/StaticHPKPins.h

if (arguments.length != 3) {
  throw "Usage: genHPKPStaticPins.js " +
        "<absolute path to PreloadedHPKPins.json> " +
        "<absolute path to default-ee.der> " +
        "<absolute path to StaticHPKPins.h>";
}

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu, 'results': Cr } = Components;

let { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
let { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

let gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                .getService(Ci.nsIX509CertDB);
gCertDB.QueryInterface(Ci.nsIX509CertDB);

const BUILT_IN_NICK_PREFIX = "Builtin Object Token:";
const SHA1_PREFIX = "sha1/";
const SHA256_PREFIX = "sha256/";
const GOOGLE_PIN_PREFIX = "GOOGLE_PIN_";

// Pins expire in 14 weeks (6 weeks on Beta + 8 weeks on stable)
const PINNING_MINIMUM_REQUIRED_MAX_AGE = 60 * 60 * 24 * 7 * 14;

const FILE_HEADER = "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
" * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
" * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
"\n" +
"/*****************************************************************************/\n" +
"/* This is an automatically generated file. If you're not                    */\n" +
"/* PublicKeyPinningService.cpp, you shouldn't be #including it.              */\n" +
"/*****************************************************************************/\n" +
"#include <stdint.h>" +
"\n";

const DOMAINHEADER = "/* Domainlist */\n" +
  "struct TransportSecurityPreload {\n" +
  "  const char* mHost;\n" +
  "  const bool mIncludeSubdomains;\n" +
  "  const bool mTestMode;\n" +
  "  const bool mIsMoz;\n" +
  "  const int32_t mId;\n" +
  "  const StaticPinset *pinset;\n" +
  "};\n\n";

const PINSETDEF = "/* Pinsets are each an ordered list by the actual value of the fingerprint */\n" +
  "struct StaticFingerprints {\n" +
  "  const size_t size;\n" +
  "  const char* const* data;\n" +
  "};\n\n" +
  "struct StaticPinset {\n" +
  "  const StaticFingerprints* sha1;\n" +
  "  const StaticFingerprints* sha256;\n" +
  "};\n\n";

// Command-line arguments
var gStaticPins = parseJson(arguments[0]);
var gTestCertFile = arguments[1];

// Open the output file.
let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(arguments[2]);
let gFileOutputStream = FileUtils.openSafeFileOutputStream(file);

function writeString(string) {
  gFileOutputStream.write(string, string.length);
}

function readFileToString(filename) {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(filename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(Ci.nsIFileInputStream);
  stream.init(file, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

function stripComments(buf) {
  var lines = buf.split("\n");
  let entryRegex = /^\s*\/\//;
  let data = "";
  for (let i = 0; i < lines.length; ++i) {
    let match = entryRegex.exec(lines[i]);
    if (!match) {
      data = data + lines[i];
    }
  }
  return data;
}

function isBuiltinToken(tokenName) {
  return tokenName == "Builtin Object Token";
}

function isCertBuiltIn(cert) {
  let tokenNames = cert.getAllTokenNames({});
  if (!tokenNames) {
    return false;
  }
  if (tokenNames.some(isBuiltinToken)) {
    return true;
  }
  return false;
}

function download(filename) {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", filename, false); // doing the request synchronously
  try {
    req.send();
  }
  catch (e) {
    throw "ERROR: problem downloading '" + filename + "': " + e;
  }

  if (req.status != 200) {
    throw("ERROR: problem downloading '" + filename + "': status " +
          req.status);
  }
  return req.responseText;
}

function downloadAsJson(filename) {
  // we have to filter out '//' comments
  var result = download(filename).replace(/\/\/[^\n]*\n/g, "");
  var data = null;
  try {
    data = JSON.parse(result);
  }
  catch (e) {
    throw "ERROR: could not parse data from '" + filename + "': " + e;
  }
  return data;
}

// Returns a Subject Public Key Digest from the given pem, if it exists.
function getSKDFromPem(pem) {
  let cert = gCertDB.constructX509FromBase64(pem, pem.length);
  return cert.sha256SubjectPublicKeyInfoDigest;
}

// Downloads the static certs file and tries to map Google Chrome nicknames
// to Mozilla nicknames, as well as storing any hashes for pins for which we
// don't have root PEMs. Each entry consists of a line containing the name of
// the pin followed either by a hash in the format "sha1/" + base64(hash), or
// a PEM encoded certificate. For certificates that we have in our database,
// return a map of Google's nickname to ours. For ones that aren't return a
// map of Google's nickname to sha1 values. This code is modeled after agl's
// https://github.com/agl/transport-security-state-generate, which doesn't
// live in the Chromium repo because go is not an official language in
// Chromium.
// For all of the entries in this file:
// - If the entry has a hash format, find the Mozilla pin name (cert nickname)
// and stick the hash into certSKDToName
// - If the entry has a PEM format, parse the PEM, find the Mozilla pin name
// and stick the hash in certSKDToName
// We MUST be able to find a corresponding cert nickname for the Chrome names,
// otherwise we skip all pinsets referring to that Chrome name.
function downloadAndParseChromeCerts(filename, certSKDToName) {
  // Prefixes that we care about.
  const BEGIN_CERT = "-----BEGIN CERTIFICATE-----";
  const END_CERT = "-----END CERTIFICATE-----";

  // Parsing states.
  const PRE_NAME = 0;
  const POST_NAME = 1;
  const IN_CERT = 2;
  let state = PRE_NAME;

  let lines = download(filename).split("\n");
  let name = "";
  let pemCert = "";
  let hash = "";
  let chromeNameToHash = {};
  let chromeNameToMozName = {}
  for (let i = 0; i < lines.length; ++i) {
    let line = lines[i];
    // Skip comments and newlines.
    if (line.length == 0 || line[0] == '#') {
      continue;
    }
    switch(state) {
      case PRE_NAME:
        chromeName = line;
        state = POST_NAME;
        break;
      case POST_NAME:
        if (line.startsWith(SHA1_PREFIX) ||
            line.startsWith(SHA256_PREFIX)) {
          if (line.startsWith(SHA1_PREFIX)) {
            hash = line.substring(SHA1_PREFIX.length);
          } else if (line.startsWith(SHA256_PREFIX)) {
            hash = line.substring(SHA256_PREFIX);
          }
          // Store the entire prefixed hash, so we can disambiguate sha1 from
          // sha256 later.
          chromeNameToHash[chromeName] = line;
          certNameToSKD[chromeName] = hash;
          certSKDToName[hash] = chromeName;
          state = PRE_NAME;
        } else if (line.startsWith(BEGIN_CERT)) {
          state = IN_CERT;
        } else {
          throw "ERROR: couldn't parse Chrome certificate file " + line;
        }
        break;
      case IN_CERT:
        if (line.startsWith(END_CERT)) {
          state = PRE_NAME;
          hash = getSKDFromPem(pemCert);
          pemCert = "";
          if (hash in certSKDToName) {
            mozName = certSKDToName[hash];
          } else {
            // Not one of our built-in certs. Prefix the name with
            // GOOGLE_PIN_.
            mozName = GOOGLE_PIN_PREFIX + chromeName;
            dump("Can't find hash in builtin certs for Chrome nickname " +
                 chromeName + ", inserting " + mozName + "\n");
            certSKDToName[hash] = mozName;
            certNameToSKD[mozName] = hash;
          }
          chromeNameToMozName[chromeName] = mozName;
        } else {
          pemCert += line;
        }
        break;
      default:
        throw "ERROR: couldn't parse Chrome certificate file " + line;
    }
  }
  return [ chromeNameToHash, chromeNameToMozName ];
}

// We can only import pinsets from chrome if for every name in the pinset:
// - We have a hash from Chrome's static certificate file
// - We have a builtin cert
// If the pinset meets these requirements, we store a map array of pinset
// objects:
// {
//   pinset_name : {
//     // Array of names with entries in certNameToSKD
//     sha1_hashes: [],
//     sha256_hashes: []
//   }
// }
// and an array of imported pinset entries:
// { name: string, include_subdomains: boolean, test_mode: boolean,
//   pins: pinset_name }
function downloadAndParseChromePins(filename,
                                    chromeNameToHash,
                                    chromeNameToMozName,
                                    certNameToSKD,
                                    certSKDToName) {
  let chromePreloads = downloadAsJson(filename);
  let chromePins = chromePreloads.pinsets;
  let chromeImportedPinsets = {};
  let chromeImportedEntries = [];

  chromePins.forEach(function(pin) {
    let valid = true;
    let pinset = { name: pin.name, sha1_hashes: [], sha256_hashes: [] };
    // Translate the Chrome pinset format to ours
    pin.static_spki_hashes.forEach(function(name) {
      if (name in chromeNameToHash) {
        let hash = chromeNameToHash[name];
        if (hash.startsWith(SHA1_PREFIX)) {
          hash = hash.substring(SHA1_PREFIX.length);
          pinset.sha1_hashes.push(certSKDToName[hash]);
        } else if (hash.startsWith(SHA256_PREFIX)) {
          hash = hash.substring(SHA256_PREFIX.length);
          pinset.sha256_hashes.push(certSKDToName[hash]);
        } else {
          throw("Unsupported hash type: " + chromeNameToHash[name]);
        }
        // We should have already added hashes for all of these when we
        // imported the certificate file.
        if (!certNameToSKD[name]) {
          throw("No hash for name: " + name);
        }
      } else if (name in chromeNameToMozName) {
        pinset.sha256_hashes.push(chromeNameToMozName[name]);
      } else {
        dump("Skipping Chrome pinset " + pinset.name + ", couldn't find " +
             "builtin " + name + " from cert file\n");
        valid = false;
      }
    });
    if (valid) {
      chromeImportedPinsets[pinset.name] = pinset;
    }
  });

  // Grab the domain entry lists. Chrome's entry format is similar to
  // ours, except theirs includes a HSTS mode.
  const cData = gStaticPins.chromium_data;
  let entries = chromePreloads.entries;
  entries.forEach(function(entry) {
    let pinsetName = cData.substitute_pinsets[entry.pins];
    if (!pinsetName) {
      pinsetName = entry.pins;
    }
    let isProductionDomain =
      (cData.production_domains.indexOf(entry.name) != -1);
    let isProductionPinset =
      (cData.production_pinsets.indexOf(pinsetName) != -1);
    let excludeDomain =
      (cData.exclude_domains.indexOf(entry.name) != -1);
    let isTestMode = !isProductionPinset && !isProductionDomain;
    if (entry.pins && !excludeDomain && chromeImportedPinsets[entry.pins]) {
      chromeImportedEntries.push({
        name: entry.name,
        include_subdomains: entry.include_subdomains,
        test_mode: isTestMode,
        is_moz: false,
        pins: pinsetName });
    }
  });
  return [ chromeImportedPinsets, chromeImportedEntries ];
}

// Returns a pair of maps [certNameToSKD, certSKDToName] between cert
// nicknames and digests of the SPKInfo for the mozilla trust store
function loadNSSCertinfo(derTestFile) {
  let allCerts = gCertDB.getCerts();
  let enumerator = allCerts.getEnumerator();
  let certNameToSKD = {};
  let certSKDToName = {};
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (!isCertBuiltIn(cert)) {
      continue;
    }
    let name = cert.nickname.substr(BUILT_IN_NICK_PREFIX.length);
    let SKD = cert.sha256SubjectPublicKeyInfoDigest;
    certNameToSKD[name] = SKD;
    certSKDToName[SKD] = name;
  }
  {
    // A certificate for *.example.com.
    let der = readFileToString(derTestFile);
    let testCert = gCertDB.constructX509(der, der.length);
    // We can't include this cert in the previous loop, because it skips
    // non-builtin certs and the nickname is not built-in to the cert.
    let name = "End Entity Test Cert";
    let SKD  = testCert.sha256SubjectPublicKeyInfoDigest;
    certNameToSKD[name] = SKD;
    certSKDToName[SKD] = name;
  }
  return [certNameToSKD, certSKDToName];
}

function parseJson(filename) {
  let json = stripComments(readFileToString(filename));
  return JSON.parse(json);
}

function nameToAlias(certName) {
  // change the name to a string valid as a c identifier
  // remove  non-ascii characters
  certName = certName.replace( /[^[:ascii:]]/g, "_");
  // replace non word characters
  certName = certName.replace(/[^A-Za-z0-9]/g ,"_");

  return "k" + certName + "Fingerprint";
}

function compareByName (a, b) {
  return a.name.localeCompare(b.name);
}

function genExpirationTime() {
  let now = new Date();
  let nowMillis = now.getTime();
  let expirationMillis = nowMillis + (PINNING_MINIMUM_REQUIRED_MAX_AGE * 1000);
  let expirationMicros = expirationMillis * 1000;
  return "static const PRTime kPreloadPKPinsExpirationTime = INT64_C(" +
         expirationMicros +");\n";
}

function writeFullPinset(certNameToSKD, certSKDToName, pinset) {
  // We aren't guaranteed to have sha1 hashes in our own imported pins.
  let prefix = "kPinset_" + pinset.name;
  let sha1Name = "nullptr";
  let sha256Name = "nullptr";
  if (pinset.sha1_hashes && pinset.sha1_hashes.length > 0) {
    writeFingerprints(certNameToSKD, certSKDToName, pinset.name,
                      pinset.sha1_hashes, "sha1");
    sha1Name = "&" + prefix + "_sha1";
  }
  if (pinset.sha256_hashes && pinset.sha256_hashes.length > 0) {
    writeFingerprints(certNameToSKD, certSKDToName, pinset.name,
                      pinset.sha256_hashes, "sha256");
    sha256Name = "&" + prefix + "_sha256";
  }
  writeString("static const StaticPinset " + prefix + " = {\n" +
          "  " + sha1Name + ",\n  " + sha256Name + "\n};\n\n");
}

function writeFingerprints(certNameToSKD, certSKDToName, name, hashes, type) {
  let varPrefix = "kPinset_" + name + "_" + type;
  writeString("static const char* " + varPrefix + "_Data[] = {\n");
  let SKDList = [];
  for (let certName of hashes) {
    if (!(certName in certNameToSKD)) {
      throw "Can't find " + certName + " in certNameToSKD";
    }
    SKDList.push(certNameToSKD[certName]);
  }
  for (let skd of SKDList.sort()) {
    writeString("  " + nameToAlias(certSKDToName[skd]) + ",\n");
  }
  if (hashes.length == 0) {
    // ANSI C requires that an initialiser list be non-empty.
    writeString("  0\n");
  }
  writeString("};\n");
  writeString("static const StaticFingerprints " + varPrefix + " = { " +
          hashes.length + ", " + varPrefix + "_Data };\n\n");
}

function writeEntry(entry) {
  let printVal = "  { \"" + entry.name + "\",\ ";
  if (entry.include_subdomains) {
    printVal += "true, ";
  } else {
    printVal += "false, ";
  }
  // Default to test mode if not specified.
  let testMode = true;
  if (entry.hasOwnProperty("test_mode")) {
    testMode = entry.test_mode;
  }
  if (testMode) {
    printVal += "true, ";
  } else {
    printVal += "false, ";
  }
  if (entry.is_moz || (entry.pins == "mozilla")) {
    printVal += "true, ";
  } else {
    printVal += "false, ";
  }
  if (entry.id >= 256) {
    throw("Not enough buckets in histogram");
  }
  if (entry.id >= 0) {
    printVal += entry.id + ", ";
  } else {
    printVal += "-1, ";
  }
  printVal += "&kPinset_" + entry.pins;
  printVal += " },\n";
  writeString(printVal);
}

function writeDomainList(chromeImportedEntries) {
  writeString("/* Sort hostnames for binary search. */\n");
  writeString("static const TransportSecurityPreload " +
          "kPublicKeyPinningPreloadList[] = {\n");
  let count = 0;
  let sortedEntries = gStaticPins.entries;
  sortedEntries.push.apply(sortedEntries, chromeImportedEntries);
  for (let entry of sortedEntries.sort(compareByName)) {
    count++;
    writeEntry(entry);
  }
  writeString("};\n");

  writeString("\nstatic const int kPublicKeyPinningPreloadListLength = " +
          count + ";\n");
  writeString("\nstatic const int32_t kUnknownId = -1;\n");
}

function writeFile(certNameToSKD, certSKDToName,
                   chromeImportedPinsets, chromeImportedEntries) {
  // Compute used pins from both Chrome's and our pinsets, so we can output
  // them later.
  usedFingerprints = {};
  gStaticPins.pinsets.forEach(function(pinset) {
    // We aren't guaranteed to have sha1_hashes in our own JSON.
    if (pinset.sha1_hashes) {
      pinset.sha1_hashes.forEach(function(name) {
        usedFingerprints[name] = true;
      });
    }
    if (pinset.sha256_hashes) {
      pinset.sha256_hashes.forEach(function(name) {
        usedFingerprints[name] = true;
      });
    }
  });
  for (let key in chromeImportedPinsets) {
    let pinset = chromeImportedPinsets[key];
    pinset.sha1_hashes.forEach(function(name) {
      usedFingerprints[name] = true;
    });
    pinset.sha256_hashes.forEach(function(name) {
      usedFingerprints[name] = true;
    });
  }

  writeString(FILE_HEADER);

  // Write actual fingerprints.
  Object.keys(usedFingerprints).sort().forEach(function(certName) {
    if (certName) {
      writeString("/* " + certName + " */\n");
      writeString("static const char " + nameToAlias(certName) + "[] =\n");
      writeString("  \"" + certNameToSKD[certName] + "\";\n");
      writeString("\n");
    }
  });

  // Write the pinsets
  writeString(PINSETDEF);
  writeString("/* PreloadedHPKPins.json pinsets */\n");
  gStaticPins.pinsets.sort(compareByName).forEach(function(pinset) {
    writeFullPinset(certNameToSKD, certSKDToName, pinset);
  });
  writeString("/* Chrome static pinsets */\n");
  for (let key in chromeImportedPinsets) {
    writeFullPinset(certNameToSKD, certSKDToName, chromeImportedPinsets[key]);
  }

  // Write the domainlist entries.
  writeString(DOMAINHEADER);
  writeDomainList(chromeImportedEntries);
  writeString("\n");
  writeString(genExpirationTime());
}

let [ certNameToSKD, certSKDToName ] = loadNSSCertinfo(gTestCertFile);
let [ chromeNameToHash, chromeNameToMozName ] = downloadAndParseChromeCerts(
  gStaticPins.chromium_data.cert_file_url, certSKDToName);
let [ chromeImportedPinsets, chromeImportedEntries ] =
  downloadAndParseChromePins(gStaticPins.chromium_data.json_file_url,
    chromeNameToHash, chromeNameToMozName, certNameToSKD, certSKDToName);

writeFile(certNameToSKD, certSKDToName, chromeImportedPinsets,
          chromeImportedEntries);

FileUtils.closeSafeFileOutputStream(gFileOutputStream);
