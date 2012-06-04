/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://services-aitc/storage.js");

const START_PORT = 8080;
const SERVER = "http://localhost";

let fakeApp1 = {
  origin: SERVER + ":" + START_PORT,
  receipts: [],
  manifestURL: "/manifest.webapp",
  installOrigin: "http://localhost",
  installedAt: Date.now(),
  modifiedAt: Date.now()
};

// Valid manifest for app1
let manifest1 = {
  name: "Appasaurus",
  description: "Best fake app ever",
  launch_path: "/",
  fullscreen: true,
  required_features: ["webgl"]
};

let fakeApp2 = {
  origin: SERVER + ":" + (START_PORT + 1),
  receipts: ["fake.jwt.token"],
  manifestURL: "/manifest.webapp",
  installOrigin: "http://localhost",
  installedAt: Date.now(),
  modifiedAt: Date.now()
};

// Invalid manifest for app2
let manifest2_bad = {
  not: "a manifest",
  fullscreen: true
};

// Valid manifest for app2
let manifest2_good = {
  name: "Supercalifragilisticexpialidocious",
  description: "Did we blow your mind yet?",
  launch_path: "/"
};

let fakeApp3 = {
  origin: SERVER + ":" + (START_PORT + 3), // 8082 is for the good manifest2
  receipts: [],
  manifestURL: "/manifest.webapp",
  installOrigin: "http://localhost",
  installedAt: Date.now(),
  modifiedAt: Date.now()
};

let manifest3 = {
  name: "Taumatawhakatangihangakoauauotamateapokaiwhenuakitanatahu",
  description: "Find your way around this beautiful hill",
  launch_path: "/"
};

function create_servers() {
  // Setup servers to server manifests at each port
  let manifests = [manifest1, manifest2_bad, manifest2_good, manifest3];
  for (let i = 0; i < manifests.length; i++) {
    let response = JSON.stringify(manifests[i]);
    httpd_setup({"/manifest.webapp": function(req, res) {
      res.setStatusLine(req.httpVersion, 200, "OK");
      res.setHeader("Content-Type", "application/x-web-app-manifest+json");
      res.bodyOutputStream.write(response, response.length);
    }}, START_PORT + i);
  }
}

function run_test() {
  create_servers();
  run_next_test();
}

add_test(function test_storage_install() {
  let apps = [fakeApp1, fakeApp2];
  AitcStorage.processApps(apps, function() {
    // Verify that app1 got added to registry
    let id = DOMApplicationRegistry._appId(fakeApp1.origin);
    do_check_eq(DOMApplicationRegistry.itemExists(id), true);

    // app2 should be missing because of bad manifest
    do_check_eq(DOMApplicationRegistry._appId(fakeApp2.origin), null);

    // Now associate fakeApp2 with a good manifest and process again
    fakeApp2.origin = SERVER + ":8082";
    AitcStorage.processApps([fakeApp1, fakeApp2], function() {
      // Both apps must be installed
      let id1 = DOMApplicationRegistry._appId(fakeApp1.origin);
      let id2 = DOMApplicationRegistry._appId(fakeApp2.origin);
      do_check_eq(DOMApplicationRegistry.itemExists(id1), true);
      do_check_eq(DOMApplicationRegistry.itemExists(id2), true);
      run_next_test();
    });
  });
});

add_test(function test_storage_uninstall() {
  // Set app1 as deleted.
  fakeApp1.deleted = true;
  AitcStorage.processApps([fakeApp2], function() {
    // It should be missing.
    do_check_eq(DOMApplicationRegistry._appId(fakeApp1.origin), null);
    run_next_test();
  });
});

add_test(function test_storage_uninstall_empty() {
  // Now remove app2 by virtue of it missing in the remote list.
  AitcStorage.processApps([fakeApp3], function() {
    let id3 = DOMApplicationRegistry._appId(fakeApp3.origin);
    do_check_eq(DOMApplicationRegistry.itemExists(id3), true);
    do_check_eq(DOMApplicationRegistry._appId(fakeApp2.origin), null);
    run_next_test();
  });
});