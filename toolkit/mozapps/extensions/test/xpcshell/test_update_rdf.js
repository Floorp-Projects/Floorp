"use strict";

// Tests the update RDF to JSON converter.

ChromeUtils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateRDFConverter",
                               "resource://gre/modules/addons/RDFManifestConverter.jsm");

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

testserver.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = "http://example.com/data";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

function checkUpdates(aId, aUpdateFile) {
  return new Promise((resolve, reject) => {
    AddonUpdateChecker.checkForUpdates(aId, `${BASE_URL}/${aUpdateFile}`, {
      onUpdateCheckComplete: resolve,

      onUpdateCheckError(status) {
        let error = new Error("Update check failed with status " + status);
        error.status = status;
        reject(error);
      }
    });
  });
}

function makeRequest(url) {
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", url);
    xhr.addEventListener("load", () => resolve(xhr), {once: true});
    xhr.addEventListener("error", reject, {once: true});
    xhr.send();
  });
}

add_task(async function test_update_rdf_to_json() {
  for (let base of ["test_update", "test_updatecheck"]) {
    info(`Testing ${base}.rdf`);

    let rdfRequest = await makeRequest(`${BASE_URL}/${base}.rdf`);
    let jsonRequest = await makeRequest(`${BASE_URL}/${base}_rdf.json`);

    let rdfJSON = UpdateRDFConverter.convertToJSON(rdfRequest);
    let json = JSON.parse(jsonRequest.responseText);

    deepEqual(Object.keys(rdfJSON).sort(), Object.keys(json).sort(),
              "Should have the same keys");
    deepEqual(Object.keys(rdfJSON.addons).sort(), Object.keys(json.addons).sort(),
              "Should have the same add-on IDs");

    for (let addon of Object.keys(json.addons)) {
      deepEqual(rdfJSON.addons[addon], json.addons[addon],
                `Should have the same entry for add-on ${addon}`);
    }

  }
});

add_task(async function test_update_check() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    let updates = await checkUpdates("updatecheck1@tests.mozilla.org", file);

    equal(updates.length, 5);
    let update = await AddonUpdateChecker.getNewestCompatibleUpdate(updates);
    notEqual(update, null);
    equal(update.version, "3.0");
    update = AddonUpdateChecker.getCompatibilityUpdate(updates, "2");
    notEqual(update, null);
    equal(update.version, "2.0");
    equal(update.targetApplications[0].minVersion, "1");
    equal(update.targetApplications[0].maxVersion, "2");
  }
});
