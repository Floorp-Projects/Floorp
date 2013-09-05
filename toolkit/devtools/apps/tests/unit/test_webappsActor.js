/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/osfile.jsm");

let gAppId = "actor-test";
const APP_ORIGIN = "app://" + gAppId;

add_test(function testLaunchInexistantApp() {
  let request = {type: "launch", manifestURL: "http://foo.com"};
  webappActorRequest(request, function (aResponse) {
    do_check_eq(aResponse.error, "NO_SUCH_APP");
    run_next_test();
  });
});

add_test(function testCloseInexistantApp() {
  let request = {type: "close", manifestURL: "http://foo.com"};
  webappActorRequest(request, function (aResponse) {
    do_check_eq(aResponse.error, "missingParameter");
    do_check_eq(aResponse.message, "No application for http://foo.com");
    run_next_test();
  });
});

// Install a test app
add_test(function testInstallPackaged() {
  // Copy our test webapp to tmp folder, where the actor retrieves it
  let zip = do_get_file("data/app.zip");
  let appDir = FileUtils.getDir("TmpD", ["b2g", gAppId], true, true);
  zip.copyTo(appDir, "application.zip");

  let request = {type: "install", appId: gAppId};
  webappActorRequest(request, function (aResponse) {
    do_check_eq(aResponse.appId, gAppId);
  });

  // The install request is asynchronous and send back an event to tell
  // if the installation succeed or failed
  gClient.addOneTimeListener("webappsEvent", function listener(aState, aType, aPacket) {
    do_check_eq(aType.appId, gAppId);
    if ("error" in aType) {
      do_print("Error: " + aType.error);
    }
    if ("message" in aType) {
      do_print("Error message: " + aType.message);
    }
    do_check_eq("error" in aType, false);

    run_next_test();
  });
});


// Now check that the app appear in getAll
add_test(function testGetAll() {
  let request = {type: "getAll"};
  webappActorRequest(request, function (aResponse) {
    do_check_true("apps" in aResponse);
    let apps = aResponse.apps;
    do_check_true(apps.length > 0);
    for (let i = 0; i < apps.length; i++) {
      let app = apps[i];
      if (app.id == gAppId) {
        do_check_eq(app.name, "Test app");
        do_check_eq(app.manifest.description, "Testing webapps actor");
        do_check_eq(app.manifest.launch_path, "/index.html");
        do_check_eq(app.origin, "app://" + gAppId);
        do_check_eq(app.installOrigin, app.origin);
        do_check_eq(app.manifestURL, app.origin + "/manifest.webapp");
        run_next_test();
        return;
      }
    }
    do_throw("Unable to find the test app by its id");
  });
});

add_test(function testLaunchApp() {
  let manifestURL = "app://" + gAppId + "/manifest.webapp";
  let startPoint = "/index.html";
  let request = {
    type: "launch",
    manifestURL: manifestURL,
    startPoint: startPoint
  };
  Services.obs.addObserver(function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);
    let json = JSON.parse(data);
    do_check_eq(json.manifestURL, manifestURL);
    do_check_eq(json.startPoint, startPoint);
    run_next_test();
  }, "webapps-launch", false);

  webappActorRequest(request, function (aResponse) {
    do_check_false("error" in aResponse);
  });
});

add_test(function testCloseApp() {
  let manifestURL = "app://" + gAppId + "/manifest.webapp";
  let request = {
    type: "close",
    manifestURL: manifestURL
  };
  Services.obs.addObserver(function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);
    let json = JSON.parse(data);
    do_check_eq(json.manifestURL, manifestURL);

  }, "webapps-close", false);

  webappActorRequest(request, function (aResponse) {
    do_check_false("error" in aResponse);
    run_next_test();
  });
});

// The 128px icon is a single red pixel and the 64px one is a blue one
// bug 899177: there is a bug with xhr and app:// and jar:// uris
// that ends up forcing the content type to application/xml
let red1px =  "data:application/xml;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4z8AAAAMBAQAY3Y2wAAAAAElFTkSuQmCC";
let blue1px = "data:application/xml;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12MwZDgHAAFlAQBDpjhLAAAAAElFTkSuQmCC";

add_test(function testGetIcon() {
  let manifestURL = APP_ORIGIN + "/manifest.webapp";
  let request = {
    type: "getIconAsDataURL",
    manifestURL: manifestURL
  };

  webappActorRequest(request, function (aResponse) {
    do_check_false("error" in aResponse);

    // By default, getIconAsDataURL return the 128x128 icon
    do_check_eq(aResponse.url, red1px);
    run_next_test();
  });
});

add_test(function testGetIconWithCustomSize() {
  let manifestURL = APP_ORIGIN + "/manifest.webapp";
  let request = {
    type: "getIconAsDataURL",
    manifestURL: manifestURL,
    size: 64
  };

  webappActorRequest(request, function (aResponse) {
    do_check_false("error" in aResponse);

    do_check_eq(aResponse.url, blue1px);
    run_next_test();
  });
});

add_test(function testUninstall() {
  let origin = "app://" + gAppId;
  let manifestURL = origin + "/manifest.webapp";
  let request = {
    type: "uninstall",
    manifestURL: manifestURL
  };

  Services.obs.addObserver(function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);
    let json = JSON.parse(data);
    do_check_eq(json.manifestURL, manifestURL);
    do_check_eq(json.origin, origin);
    do_check_eq(json.id, gAppId);
    run_next_test();
  }, "webapps-uninstall", false);

  webappActorRequest(request, function (aResponse) {
    do_check_false("error" in aResponse);
  });
});

add_test(function testFileUploadInstall() {
  function createUpload() {
    let request = {
      type: "uploadPackage"
    };
    webappActorRequest(request, function (aResponse) {
      getPackageContent(aResponse.actor);
    });
  }
  function getPackageContent(uploadActor) {
    let packageFile = do_get_file("data/app.zip");
    OS.File.read(packageFile.path)
      .then(function (bytes) {
        // To work around the fact that JSON.stringify translates the typed
        // array to object, we are encoding the typed array here into a string
        let content = String.fromCharCode.apply(null, bytes);
        uploadChunk(uploadActor, content);
      });
  }
  function uploadChunk(uploadActor, content) {
    let request = {
      to: uploadActor,
      type: "chunk",
      chunk: content
    };
    gClient.request(request, function (aResponse) {
      endsUpload(uploadActor);
    });
  }
  function endsUpload(uploadActor, content) {
    let request = {
      to: uploadActor,
      type: "done"
    };
    gClient.request(request, function (aResponse) {
      installApp(uploadActor);
    });
  }
  function installApp(uploadActor) {
    let request = {type: "install", appId: gAppId, upload: uploadActor};
    webappActorRequest(request, function (aResponse) {
      do_check_eq(aResponse.appId, gAppId);
    });
    gClient.addOneTimeListener("webappsEvent", function listener(aState, aType, aPacket) {
      do_check_eq(aType.appId, gAppId);
      if ("error" in aType) {
        do_print("Error: " + aType.error);
      }
      if ("message" in aType) {
        do_print("Error message: " + aType.message);
      }
      do_check_eq("error" in aType, false);

      removeUpload(uploadActor);
    });
  }
  function removeUpload(uploadActor, content) {
    let request = {
      to: uploadActor,
      type: "remove"
    };
    gClient.request(request, function (aResponse) {
      run_next_test();
    });
  }
  createUpload();
});

add_test(function testInstallHosted() {
  gAppId = "hosted-app";
  let request = {
    type: "install",
    appId: gAppId,
    manifest: {
      name: "My hosted app"
    },
    metadata: {
      origin: "http://foo.com",
      installOrigin: "http://metadata.foo.com",
      manifestURL: "http://foo.com/metadata/manifest.webapp"
    }
  };
  webappActorRequest(request, function (aResponse) {
    do_check_eq(aResponse.appId, gAppId);
  });

  // The install request is asynchronous and send back an event to tell
  // if the installation succeed or failed
  gClient.addOneTimeListener("webappsEvent", function listener(aState, aType, aPacket) {
    do_check_eq(aType.appId, gAppId);
    if ("error" in aType) {
      do_print("Error: " + aType.error);
    }
    if ("message" in aType) {
      do_print("Error message: " + aType.message);
    }
    do_check_eq("error" in aType, false);

    run_next_test();
  });
});

add_test(function testCheckHostedApp() {
  let request = {type: "getAll"};
  webappActorRequest(request, function (aResponse) {
    do_check_true("apps" in aResponse);
    let apps = aResponse.apps;
    do_check_true(apps.length > 0);
    for (let i = 0; i < apps.length; i++) {
      let app = apps[i];
      if (app.id == gAppId) {
        do_check_eq(app.name, "My hosted app");
        do_check_eq(app.origin, "http://foo.com");
        do_check_eq(app.installOrigin, "http://metadata.foo.com");
        do_check_eq(app.manifestURL, "http://foo.com/metadata/manifest.webapp");
        run_next_test();
        return;
      }
    }
    do_throw("Unable to find the test app by its id");
  });
});

function run_test() {
  setup();

  run_next_test();
}

