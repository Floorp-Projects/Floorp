Cu.import("resource://webapprt/modules/WebappRT.jsm");
let { AppsUtils } = Cu.import("resource://gre/modules/AppsUtils.jsm", {});
let { DOMApplicationRegistry } =
  Cu.import("resource://gre/modules/Webapps.jsm", {});
let { PermissionsTable, PermissionsReverseTable } =
  Cu.import("resource://gre/modules/PermissionsTable.jsm", {});

function test() {
  waitForExplicitFinish();

  loadWebapp("webperm.webapp", undefined, function onLoad() {
    let app = WebappRT.config.app;

    // Check that the app is non privileged.
    is(AppsUtils.getAppManifestStatus(app.manifest), Ci.nsIPrincipal.APP_STATUS_INSTALLED, "The app is not privileged");

    // Check that the app principal has the correct appId.
    let principal = document.getElementById("content").contentDocument.defaultView.document.nodePrincipal;
    is(DOMApplicationRegistry.getAppLocalIdByManifestURL(app.manifestURL), principal.appId, "Principal app ID correct");

    let perms = [ "indexedDB-unlimited", "geolocation",
                  "camera", "alarms", "tcp-socket", "network-events",
                  "webapps-manage", "desktop-notification" ];

    for (let permName of perms) {
      // Get the values for all the permission.
      let permValue = Services.perms.testExactPermissionFromPrincipal(principal, permName);

      // Check if the app has the permission as specified in the PermissionsTable.jsm file.
      let realPerm = PermissionsReverseTable[permName];
      is(permValue, PermissionsTable[realPerm]["app"], "Permission " + permName + " correctly set.");
    }

    finish();
  });
}
