/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file is a helper script that generates the list of certificates that
// make up the preloaded pinset for Google properties.
//
// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell dumpGoogleRoots.js'
// 4. [paste the output into the appropriate section in
//     security/manager/tools/PreloadedHPKPins.json]

Cu.importGlobalProperties(["XMLHttpRequest"]);

function downloadRoots() {
  let req = new XMLHttpRequest();
  req.open("GET", "https://pki.google.com/roots.pem", false);
  try {
    req.send();
  } catch (e) {
    throw new Error("ERROR: problem downloading Google Root PEMs: " + e);
  }

  if (req.status != 200) {
    throw new Error("ERROR: problem downloading Google Root PEMs. Status: " +
                    req.status);
  }

  let pem = req.responseText;
  let roots = [];
  let currentPEM = "";
  let readingRoot = false;
  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  for (let line of pem.split(/[\r\n]/)) {
    if (line == "-----END CERTIFICATE-----") {
      if (currentPEM) {
        roots.push(certDB.constructX509FromBase64(currentPEM));
      }
      currentPEM = "";
      readingRoot = false;
      continue;
    }
    if (readingRoot) {
      currentPEM += line;
    }
    if (line == "-----BEGIN CERTIFICATE-----") {
      readingRoot = true;
    }
  }
  return roots;
}

function makeFormattedNickname(cert) {
  if (cert.isBuiltInRoot) {
    return `"${cert.displayName}"`;
  }
  // Otherwise, this isn't a built-in and we have to comment it out.
  return `// "${cert.displayName}"`;
}

var roots = downloadRoots();
var rootNicknames = [];
for (var root of roots) {
  rootNicknames.push(makeFormattedNickname(root));
}
rootNicknames.sort(function(rootA, rootB) {
  let rootALowercase = rootA.toLowerCase().replace(/(^[^"]*")|"/g, "");
  let rootBLowercase = rootB.toLowerCase().replace(/(^[^"]*")|"/g, "");
  if (rootALowercase < rootBLowercase) {
    return -1;
  }
  if (rootALowercase > rootBLowercase) {
    return 1;
  }
  return 0;
});
dump("    {\n");
dump("      \"name\": \"google_root_pems\",\n");
dump("      \"sha256_hashes\": [\n");
var first = true;
for (var nickname of rootNicknames) {
  if (!first) {
    dump(",\n");
  }
  first = false;
  dump("        " + nickname);
}
dump("\n");
dump("      ]\n");
dump("    }\n");
