/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell \
//                                  [path to]/genHPKPStaticpins.js \
//                                  [absolute path to]/PreloadedHPKPins.json \
//                                  [an unused argument - see bug 1205406] \
//                                  [absolute path to]/StaticHPKPins.h
"use strict";

if (arguments.length != 3) {
  throw new Error(
    "Usage: genHPKPStaticPins.js " +
      "<absolute path to PreloadedHPKPins.json> " +
      "<an unused argument - see bug 1205406> " +
      "<absolute path to StaticHPKPins.h>"
  );
}

var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

var gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const SHA256_PREFIX = "sha256/";
const GOOGLE_PIN_PREFIX = "GOOGLE_PIN_";

// Pins expire in 14 weeks (6 weeks on Beta + 8 weeks on stable)
const PINNING_MINIMUM_REQUIRED_MAX_AGE = 60 * 60 * 24 * 7 * 14;

const FILE_HEADER =
  "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
  " * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
  " * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
  "\n" +
  "/*****************************************************************************/\n" +
  "/* This is an automatically generated file. If you're not                    */\n" +
  "/* PublicKeyPinningService.cpp, you shouldn't be #including it.              */\n" +
  "/*****************************************************************************/\n" +
  "#include <stdint.h>" +
  "\n";

const DOMAINHEADER =
  "/* Domainlist */\n" +
  "struct TransportSecurityPreload {\n" +
  "  // See bug 1338873 about making these fields const.\n" +
  "  const char* mHost;\n" +
  "  bool mIncludeSubdomains;\n" +
  "  bool mTestMode;\n" +
  "  bool mIsMoz;\n" +
  "  int32_t mId;\n" +
  "  const StaticFingerprints* pinset;\n" +
  "};\n\n";

const PINSETDEF =
  "/* Pinsets are each an ordered list by the actual value of the fingerprint */\n" +
  "struct StaticFingerprints {\n" +
  "  // See bug 1338873 about making these fields const.\n" +
  "  size_t size;\n" +
  "  const char* const* data;\n" +
  "};\n\n";

// Command-line arguments
var gStaticPins = parseJson(arguments[0]);

// arguments[1] is ignored for now. See bug 1205406.

// Open the output file.
var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
file.initWithPath(arguments[2]);
var gFileOutputStream = FileUtils.openSafeFileOutputStream(file);

function writeString(string) {
  gFileOutputStream.write(string, string.length);
}

function readFileToString(filename) {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(filename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  stream.init(file, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

function stripComments(buf) {
  let lines = buf.split("\n");
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

function download(filename) {
  let req = new XMLHttpRequest();
  req.open("GET", filename, false); // doing the request synchronously
  try {
    req.send();
  } catch (e) {
    throw new Error(`ERROR: problem downloading '${filename}': ${e}`);
  }

  if (req.status != 200) {
    throw new Error(
      "ERROR: problem downloading '" + filename + "': status " + req.status
    );
  }

  let resultDecoded;
  try {
    resultDecoded = atob(req.responseText);
  } catch (e) {
    throw new Error(
      "ERROR: could not decode data as base64 from '" + filename + "': " + e
    );
  }
  return resultDecoded;
}

function downloadAsJson(filename) {
  // we have to filter out '//' comments, while not mangling the json
  let result = download(filename).replace(/^(\s*)?\/\/[^\n]*\n/gm, "");
  let data = null;
  try {
    data = JSON.parse(result);
  } catch (e) {
    throw new Error(
      "ERROR: could not parse data from '" + filename + "': " + e
    );
  }
  return data;
}

// Returns a Subject Public Key Digest from the given pem, if it exists.
function getSKDFromPem(pem) {
  let cert = gCertDB.constructX509FromBase64(pem, pem.length);
  return cert.sha256SubjectPublicKeyInfoDigest;
}

/**
 * Hashes |input| using the SHA-256 algorithm in the following manner:
 *   btoa(sha256(atob(input)))
 *
 * @argument {String} input Base64 string to decode and return the hash of.
 * @returns {String} Base64 encoded SHA-256 hash.
 */
function sha256Base64(input) {
  let decodedValue;
  try {
    decodedValue = atob(input);
  } catch (e) {
    throw new Error(`ERROR: could not decode as base64: '${input}': ${e}`);
  }

  // Convert |decodedValue| to an array so that it can be hashed by the
  // nsICryptoHash instance below.
  // In most cases across the code base, convertToByteArray() of
  // nsIScriptableUnicodeConverter is used to do this, but the method doesn't
  // seem to work here.
  let data = [];
  for (let i = 0; i < decodedValue.length; i++) {
    data[i] = decodedValue.charCodeAt(i);
  }

  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  // true is passed so that the hasher returns a Base64 encoded string.
  return hasher.finish(true);
}

// Downloads the static certs file and tries to map Google Chrome nicknames
// to Mozilla nicknames, as well as storing any hashes for pins for which we
// don't have root PEMs. Each entry consists of a line containing the name of
// the pin followed either by a hash in the format "sha256/" + base64(hash),
// a PEM encoded public key, or a PEM encoded certificate.
// For certificates that we have in our database,
// return a map of Google's nickname to ours. For ones that aren't return a
// map of Google's nickname to SHA-256 values. This code is modeled after agl's
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
function downloadAndParseChromeCerts(filename, certNameToSKD, certSKDToName) {
  // Prefixes that we care about.
  const BEGIN_CERT = "-----BEGIN CERTIFICATE-----";
  const END_CERT = "-----END CERTIFICATE-----";
  const BEGIN_PUB_KEY = "-----BEGIN PUBLIC KEY-----";
  const END_PUB_KEY = "-----END PUBLIC KEY-----";

  // Parsing states.
  const PRE_NAME = 0;
  const POST_NAME = 1;
  const IN_CERT = 2;
  const IN_PUB_KEY = 3;
  let state = PRE_NAME;

  let lines = download(filename).split("\n");
  let pemCert = "";
  let pemPubKey = "";
  let hash = "";
  let chromeNameToHash = {};
  let chromeNameToMozName = {};
  let chromeName;
  for (let line of lines) {
    // Skip comments and newlines.
    if (line.length == 0 || line[0] == "#") {
      continue;
    }
    switch (state) {
      case PRE_NAME:
        chromeName = line;
        state = POST_NAME;
        break;
      case POST_NAME:
        if (line.startsWith(SHA256_PREFIX)) {
          hash = line.substring(SHA256_PREFIX.length);
          chromeNameToHash[chromeName] = hash;
          certNameToSKD[chromeName] = hash;
          certSKDToName[hash] = chromeName;
          state = PRE_NAME;
        } else if (line.startsWith(BEGIN_CERT)) {
          state = IN_CERT;
        } else if (line.startsWith(BEGIN_PUB_KEY)) {
          state = IN_PUB_KEY;
        } else {
          throw new Error(
            "ERROR: couldn't parse Chrome certificate file line: " + line
          );
        }
        break;
      case IN_CERT:
        if (line.startsWith(END_CERT)) {
          state = PRE_NAME;
          hash = getSKDFromPem(pemCert);
          pemCert = "";
          let mozName;
          if (hash in certSKDToName) {
            mozName = certSKDToName[hash];
          } else {
            // Not one of our built-in certs. Prefix the name with
            // GOOGLE_PIN_.
            mozName = GOOGLE_PIN_PREFIX + chromeName;
            dump(
              "Can't find hash in builtin certs for Chrome nickname " +
                chromeName +
                ", inserting " +
                mozName +
                "\n"
            );
            certSKDToName[hash] = mozName;
            certNameToSKD[mozName] = hash;
          }
          chromeNameToMozName[chromeName] = mozName;
        } else {
          pemCert += line;
        }
        break;
      case IN_PUB_KEY:
        if (line.startsWith(END_PUB_KEY)) {
          state = PRE_NAME;
          hash = sha256Base64(pemPubKey);
          pemPubKey = "";
          chromeNameToHash[chromeName] = hash;
          certNameToSKD[chromeName] = hash;
          certSKDToName[hash] = chromeName;
        } else {
          pemPubKey += line;
        }
        break;
      default:
        throw new Error(
          "ERROR: couldn't parse Chrome certificate file " + line
        );
    }
  }
  return [chromeNameToHash, chromeNameToMozName];
}

// We can only import pinsets from chrome if for every name in the pinset:
// - We have a hash from Chrome's static certificate file
// - We have a builtin cert
// If the pinset meets these requirements, we store a map array of pinset
// objects:
// {
//   pinset_name : {
//     // Array of names with entries in certNameToSKD
//     sha256_hashes: []
//   }
// }
// and an array of imported pinset entries:
// { name: string, include_subdomains: boolean, test_mode: boolean,
//   pins: pinset_name }
function downloadAndParseChromePins(
  filename,
  chromeNameToHash,
  chromeNameToMozName,
  certNameToSKD,
  certSKDToName
) {
  let chromePreloads = downloadAsJson(filename);
  let chromePins = chromePreloads.pinsets;
  let chromeImportedPinsets = {};
  let chromeImportedEntries = [];

  chromePins.forEach(function(pin) {
    let valid = true;
    let pinset = { name: pin.name, sha256_hashes: [] };
    // Translate the Chrome pinset format to ours
    pin.static_spki_hashes.forEach(function(name) {
      if (name in chromeNameToHash) {
        let hash = chromeNameToHash[name];
        pinset.sha256_hashes.push(certSKDToName[hash]);

        // We should have already added hashes for all of these when we
        // imported the certificate file.
        if (!certNameToSKD[name]) {
          throw new Error("ERROR: No hash for name: " + name);
        }
      } else if (name in chromeNameToMozName) {
        pinset.sha256_hashes.push(chromeNameToMozName[name]);
      } else {
        dump(
          "Skipping Chrome pinset " +
            pinset.name +
            ", couldn't find " +
            "builtin " +
            name +
            " from cert file\n"
        );
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
    // HSTS entry only
    if (!entry.pins) {
      return;
    }
    let pinsetName = cData.substitute_pinsets[entry.pins];
    if (!pinsetName) {
      pinsetName = entry.pins;
    }

    // We trim the entry name here to avoid breaking hostname comparisons in the
    // HPKP implementation.
    entry.name = entry.name.trim();

    let isProductionDomain = cData.production_domains.includes(entry.name);
    let isProductionPinset = cData.production_pinsets.includes(pinsetName);
    let excludeDomain = cData.exclude_domains.includes(entry.name);
    let isTestMode = !isProductionPinset && !isProductionDomain;
    if (entry.pins && !excludeDomain && chromeImportedPinsets[entry.pins]) {
      chromeImportedEntries.push({
        name: entry.name,
        include_subdomains: entry.include_subdomains,
        test_mode: isTestMode,
        is_moz: false,
        pins: pinsetName,
      });
    }
  });
  return [chromeImportedPinsets, chromeImportedEntries];
}

// Returns a pair of maps [certNameToSKD, certSKDToName] between cert
// nicknames and digests of the SPKInfo for the mozilla trust store
function loadNSSCertinfo(extraCertificates) {
  let allCerts = gCertDB.getCerts();
  let certNameToSKD = {};
  let certSKDToName = {};
  for (let cert of allCerts) {
    if (!cert.isBuiltInRoot) {
      continue;
    }
    let name = cert.displayName;
    let SKD = cert.sha256SubjectPublicKeyInfoDigest;
    certNameToSKD[name] = SKD;
    certSKDToName[SKD] = name;
  }

  for (let cert of extraCertificates) {
    let name = cert.commonName;
    let SKD = cert.sha256SubjectPublicKeyInfoDigest;
    certNameToSKD[name] = SKD;
    certSKDToName[SKD] = name;
  }

  {
    // This is the pinning test certificate. The key hash identifies the
    // default RSA key from pykey.
    let name = "End Entity Test Cert";
    let SKD = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
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
  certName = certName.replace(/[^[:ascii:]]/g, "_");
  // replace non word characters
  certName = certName.replace(/[^A-Za-z0-9]/g, "_");

  return "k" + certName + "Fingerprint";
}

function compareByName(a, b) {
  return a.name.localeCompare(b.name);
}

function genExpirationTime() {
  let now = new Date();
  let nowMillis = now.getTime();
  let expirationMillis = nowMillis + PINNING_MINIMUM_REQUIRED_MAX_AGE * 1000;
  let expirationMicros = expirationMillis * 1000;
  return (
    "static const PRTime kPreloadPKPinsExpirationTime = INT64_C(" +
    expirationMicros +
    ");\n"
  );
}

function writeFullPinset(certNameToSKD, certSKDToName, pinset) {
  if (!pinset.sha256_hashes || pinset.sha256_hashes.length == 0) {
    throw new Error(`ERROR: Pinset ${pinset.name} does not contain any hashes`);
  }
  writeFingerprints(
    certNameToSKD,
    certSKDToName,
    pinset.name,
    pinset.sha256_hashes
  );
}

function writeFingerprints(certNameToSKD, certSKDToName, name, hashes) {
  let varPrefix = "kPinset_" + name;
  writeString("static const char* const " + varPrefix + "_Data[] = {\n");
  let SKDList = [];
  for (let certName of hashes) {
    if (!(certName in certNameToSKD)) {
      throw new Error(`ERROR: Can't find '${certName}' in certNameToSKD`);
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
  writeString(
    "static const StaticFingerprints " +
      varPrefix +
      " = {\n  " +
      "sizeof(" +
      varPrefix +
      "_Data) / sizeof(const char*),\n  " +
      varPrefix +
      "_Data\n};\n\n"
  );
}

function writeEntry(entry) {
  let printVal = `  { "${entry.name}", `;
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
  if (
    entry.is_moz ||
    (entry.pins.includes("mozilla") && entry.pins != "mozilla_test")
  ) {
    printVal += "true, ";
  } else {
    printVal += "false, ";
  }
  if ("id" in entry) {
    if (entry.id >= 256) {
      throw new Error("ERROR: Not enough buckets in histogram");
    }
    if (entry.id >= 0) {
      printVal += entry.id + ", ";
    }
  } else {
    printVal += "-1, ";
  }
  printVal += "&kPinset_" + entry.pins;
  printVal += " },\n";
  writeString(printVal);
}

function writeDomainList(chromeImportedEntries) {
  writeString("/* Sort hostnames for binary search. */\n");
  writeString(
    "static const TransportSecurityPreload " +
      "kPublicKeyPinningPreloadList[] = {\n"
  );
  let count = 0;
  let mozillaDomains = {};
  gStaticPins.entries.forEach(function(entry) {
    mozillaDomains[entry.name] = true;
  });
  // For any domain for which we have set pins, exclude them from
  // chromeImportedEntries.
  for (let i = chromeImportedEntries.length - 1; i >= 0; i--) {
    if (mozillaDomains[chromeImportedEntries[i].name]) {
      dump(
        "Skipping duplicate pinset for domain " +
          JSON.stringify(chromeImportedEntries[i], undefined, 2) +
          "\n"
      );
      chromeImportedEntries.splice(i, 1);
    }
  }
  let sortedEntries = gStaticPins.entries;
  sortedEntries.push.apply(sortedEntries, chromeImportedEntries);
  for (let entry of sortedEntries.sort(compareByName)) {
    count++;
    writeEntry(entry);
  }
  writeString("};\n");

  writeString("\n// Pinning Preload List Length = " + count + ";\n");
  writeString("\nstatic const int32_t kUnknownId = -1;\n");
}

function writeFile(
  certNameToSKD,
  certSKDToName,
  chromeImportedPinsets,
  chromeImportedEntries
) {
  // Compute used pins from both Chrome's and our pinsets, so we can output
  // them later.
  let usedFingerprints = {};
  let mozillaPins = {};
  gStaticPins.pinsets.forEach(function(pinset) {
    mozillaPins[pinset.name] = true;
    pinset.sha256_hashes.forEach(function(name) {
      usedFingerprints[name] = true;
    });
  });
  for (let key in chromeImportedPinsets) {
    let pinset = chromeImportedPinsets[key];
    pinset.sha256_hashes.forEach(function(name) {
      usedFingerprints[name] = true;
    });
  }

  writeString(FILE_HEADER);

  // Write actual fingerprints.
  Object.keys(usedFingerprints)
    .sort()
    .forEach(function(certName) {
      if (certName) {
        writeString("/* " + certName + " */\n");
        writeString("static const char " + nameToAlias(certName) + "[] =\n");
        writeString('  "' + certNameToSKD[certName] + '";\n');
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
    if (mozillaPins[key]) {
      dump("Skipping duplicate pinset " + key + "\n");
    } else {
      dump("Writing pinset " + key + "\n");
      writeFullPinset(certNameToSKD, certSKDToName, chromeImportedPinsets[key]);
    }
  }

  // Write the domainlist entries.
  writeString(DOMAINHEADER);
  writeDomainList(chromeImportedEntries);
  writeString("\n");
  writeString(genExpirationTime());
}

function loadExtraCertificates(certStringList) {
  let constructedCerts = [];
  for (let certString of certStringList) {
    constructedCerts.push(gCertDB.constructX509FromBase64(certString));
  }
  return constructedCerts;
}

var extraCertificates = loadExtraCertificates(gStaticPins.extra_certificates);
var [certNameToSKD, certSKDToName] = loadNSSCertinfo(extraCertificates);
var [chromeNameToHash, chromeNameToMozName] = downloadAndParseChromeCerts(
  gStaticPins.chromium_data.cert_file_url,
  certNameToSKD,
  certSKDToName
);
var [chromeImportedPinsets, chromeImportedEntries] = downloadAndParseChromePins(
  gStaticPins.chromium_data.json_file_url,
  chromeNameToHash,
  chromeNameToMozName,
  certNameToSKD,
  certSKDToName
);

writeFile(
  certNameToSKD,
  certSKDToName,
  chromeImportedPinsets,
  chromeImportedEntries
);

FileUtils.closeSafeFileOutputStream(gFileOutputStream);
