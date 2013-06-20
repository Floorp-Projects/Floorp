/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell \
//                                  [path to]/genHPKPStaticpins.js

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu, 'results': Cr } = Components;

let { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
let { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
let { XPCOMUtils } = Cu.import("resource:///modules/XPCOMUtils.jsm", {});

const certdb2 = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB2);

const OUTPUT = "StaticHPKPins.h";
const MOZINPUT = "PreloadedHPKPins.json";
const PINNING_MINIMUM_REQUIRED_MAX_AGE = 60 * 60 * 24 * 7 * 18;
const FILE_HEADER = "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
" * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
" * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
"\n" +
"/*****************************************************************************/\n" +
"/* This is an automatically generated file. If you're not                    */\n" +
"/* PublicKeyPinningSerice.cpp, you shouldn't be #including it.               */\n" +
"/*****************************************************************************/\n" +
"#include <stdint.h>" +
"\n";
const DOMAINHEADER ="/*Domainlist*/\n" +
"typedef struct {\n"+
"  const char *mHost;\n"+
"  const bool mIncludeSubdomains;\n"+
"  const StaticPinset *pinset;\n"+
"} TransportSecurityPreload;\n"+
"\nstatic const TransportSecurityPreload kPublicKeyPinningPreloadList[] = {\n";
const PINSETDEF ="/*Now the pinsets, each is an ordered list by the actual value of the FP*/\n"+
"typedef struct {\n"+
"  const size_t size;\n"+
"  const char* const* data;\n"+
"} StaticPinset;\n";

function writeTo(string, fos) {
  fos.write(string, string.length);
}

function readFile(filename) {
  print("filename =" + filename)
  let path = filename;

  let lf = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("CurWorkD", Components.interfaces.nsILocalFile);

  let bits = path.split("/");
  for (let i = 0; i < bits.length; i++) {
    if (bits[i]) {
      if (bits[i] == "..") {
        lf = lf.parent;
      }
      else {
        lf.append(bits[i]);
      }
    }
  }
  let file = lf;
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  var line = {};
  let entryRegex = /^\s*\/\//;
  let data = "";
  while(fstream.readLine(line)) {
    let match = entryRegex.exec(line.value);
    if (!match) {
      data = data + line.value;
    }
  }
  // The last line of the read is a false, but content can be there
  // so: repeat the logic
  let match = entryRegex.exec(line.value);
  if (!match) {
    data = data + line.value;
  }
  fstream.close();
  return data;
}

function isBuiltinToken(tokenName) {
  return tokenName == "Builtin Object Token";
}

function isCertBuiltIn(cert) {
  let cert3 = cert.QueryInterface(Ci.nsIX509Cert3);
  let tokenNames = cert3.getAllTokenNames({});
  if (!tokenNames) {
    return false;
  }
  if (tokenNames.some(isBuiltinToken)) {
    return true;
  }
  return false;
}

// Returns a pair of maps [certNameToSKD, certSDKToName] between cert
// nicknames and digests of the SPKInfo for the mozilla trust store
function loadNSSCertinfo() {
  let allCerts = certdb2.getCerts();
  let enumerator = allCerts.getEnumerator();
  let certNameToSKD = {};
  let certSDKToName = {};
  const BUILT_IN_NICK_PREFIX = "Builtin Object Token:";
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (!isCertBuiltIn(cert)) {
      continue;
    }
    let name = cert.nickname.substr(BUILT_IN_NICK_PREFIX.length);
    let SDK  = cert.sha256SubjectPublicKeyInfoDigest;
    certNameToSKD[name] = SDK;
    certSDKToName[SDK] = name;
  }
  return [certNameToSKD, certSDKToName];
}

function parseMozFile() {
  mozFile = readFile(MOZINPUT);
  mozJSON = JSON.parse(mozFile);
  return mozJSON;
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
  return "const PRTime kPreloadPKPinsExpirationTime = INT64_C(" +
         expirationMicros +");\n";
}

function writeFile(certNameToSDK, certSDKToName, jsonPins) {
  try {
    let file = FileUtils.getFile("CurWorkD", [OUTPUT]);
    let fos = FileUtils.openSafeFileOutputStream(file);

    writeTo(FILE_HEADER, fos);

    // compute used keys
    let usedFingerPrints = {};
    let pinset = jsonPins["pinsets"];
    for (let pinsetEntry of pinset) {
      for (let certName of pinsetEntry.static_spki_hashes) {
        usedFingerPrints[certName] = true;
      }
    }
    for (let certName of Object.keys(usedFingerPrints).sort()) {
      writeTo("/* "+ certName + " */\n", fos);
      writeTo("static const char " + nameToAlias(certName) + "[]=\n", fos);
      writeTo("  \"" + certNameToSDK[certName] + "\";\n", fos);
      writeTo("\n", fos);
    }

    writeTo(PINSETDEF, fos);

    for (let pinset of jsonPins["pinsets"].sort(compareByName)) {
      writeTo("static const char* const kPinSet_" + pinset.name + "_Data[] = {\n", fos);
      let certNameList = pinset.static_spki_hashes;
      let SKDList = [];
      for (let certName of pinset.static_spki_hashes) {
        SKDList.push(certNameToSDK[certName]);
      }
      for (let sdk of SKDList.sort() ) {
        writeTo("    " + nameToAlias(certSDKToName[sdk])  + ",\n", fos);
      }
      writeTo("};\n", fos);
      writeTo("const StaticPinset kPinSet_"+ pinset.name +" = { " +
              pinset.static_spki_hashes.length + ", kPinSet_"+pinset.name +
              "_Data};\n", fos);
      writeTo("\n", fos);
    }

    // now the domainlist entries
    writeTo(DOMAINHEADER, fos);
    for (let entry of jsonPins["entries"].sort(compareByName)) {
      let printVal = "  { \""+ entry.name +"\",\t";
      if (entry.include_subdomains) {
        printVal += "true,\t";
      } else {
        printVal += "false,\t";
      }
      printVal += "&kPinSet_"+entry.pins;
      printVal += " },\n";
      writeTo(printVal, fos);
    }
    writeTo("};\n", fos);
    writeTo("\n", fos);
    let domainFooter = "static const int " +
                       "kPublicKeyPinningPreloadListLength = " +
                       jsonPins["entries"].length + ";\n";
    writeTo(domainFooter, fos);
    writeTo("\n", fos);
    writeTo(genExpirationTime(), fos);

    FileUtils.closeSafeFileOutputStream(fos);

  }
  catch (e) {
    dump("ERROR: problem writing output to '" + OUTPUT + "': " + e + "\n");
  }
}

// ****************************************************************************
// This is where the action happens:

let [certNameToSKD, certSDKToName] = loadNSSCertinfo();
let mozJSON = parseMozFile();
writeFile(certNameToSKD, certSDKToName, mozJSON);
