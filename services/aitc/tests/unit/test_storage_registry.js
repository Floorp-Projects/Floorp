/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://services-common/async.js");
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
  initTestLogging();
  create_servers();
  run_next_test();
}

add_test(function test_storage_install() {
  let apps = [fakeApp1, fakeApp2];
  AitcStorage.processApps(apps, function() {
    // Verify that app1 got added to registry
    let id = DOMApplicationRegistry._appId(fakeApp1.origin);
    do_check_true(DOMApplicationRegistry.itemExists(id));

    // app2 should be missing because of bad manifest
    do_check_null(DOMApplicationRegistry._appId(fakeApp2.origin));

    // Now associate fakeApp2 with a good manifest and process again
    fakeApp2.origin = SERVER + ":8082";
    AitcStorage.processApps([fakeApp1, fakeApp2], function() {
      // Both apps must be installed
      let id1 = DOMApplicationRegistry._appId(fakeApp1.origin);
      let id2 = DOMApplicationRegistry._appId(fakeApp2.origin);
      do_check_true(DOMApplicationRegistry.itemExists(id1));
      do_check_true(DOMApplicationRegistry.itemExists(id2));
      run_next_test();
    });
  });
});

add_test(function test_storage_uninstall() {
  _("Ensure explicit uninstalls through hidden are honored.");
  do_check_neq(DOMApplicationRegistry._appId(fakeApp1.origin), null);

  // Set app1 as hidden.
  fakeApp1.hidden = true;
  AitcStorage.processApps([fakeApp1], function() {
    // It should be missing.
    do_check_null(DOMApplicationRegistry._appId(fakeApp1.origin));
    run_next_test();
  });
});

add_test(function test_storage_uninstall_missing() {
  _("Ensure a local app with no remote record is uninstalled.");

  // If the remote server has data, any local apps not on the remote server
  // should be deleted.
  let cb = Async.makeSpinningCallback();
  AitcStorage.processApps([fakeApp2], cb);
  cb.wait();
  do_check_neq(DOMApplicationRegistry._appId(fakeApp2.origin), null);

  AitcStorage.processApps([fakeApp3], function() {
    let id3 = DOMApplicationRegistry._appId(fakeApp3.origin);
    do_check_true(DOMApplicationRegistry.itemExists(id3));
    do_check_null(DOMApplicationRegistry._appId(fakeApp2.origin));
    run_next_test();
  });
});

add_test(function test_uninstall_noop() {
  _("Ensure that an empty set of remote records does nothing.");

  let id = DOMApplicationRegistry._appId(fakeApp3.origin);
  do_check_neq(id, null);
  do_check_true(DOMApplicationRegistry.itemExists(id));

  AitcStorage.processApps([], function onComplete() {
    do_check_true(DOMApplicationRegistry.itemExists(id));

    run_next_test();
  });
});
