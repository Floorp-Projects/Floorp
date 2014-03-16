/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const modules = [
  "addonutils.js",
  "addonsreconciler.js",
  "browserid_identity.js",
  "constants.js",
  "engines/addons.js",
  "engines/bookmarks.js",
  "engines/clients.js",
  "engines/forms.js",
  "engines/history.js",
  "engines/passwords.js",
  "engines/prefs.js",
  "engines/tabs.js",
  "engines.js",
  "identity.js",
  "jpakeclient.js",
  "keys.js",
  "main.js",
  "notifications.js",
  "policies.js",
  "record.js",
  "resource.js",
  "rest.js",
  "service.js",
  "stages/cluster.js",
  "stages/declined.js",
  "stages/enginesync.js",
  "status.js",
  "userapi.js",
  "util.js",
];

const testingModules = [
  "fakeservices.js",
  "rotaryengine.js",
  "utils.js",
  "fxa_utils.js",
];

function run_test() {
  for (let m of modules) {
    let res = "resource://services-sync/" + m;
    _("Attempting to load " + res);
    Cu.import(res, {});
  }

  for (let m of testingModules) {
    let res = "resource://testing-common/services/sync/" + m;
    _("Attempting to load " + res);
    Cu.import(res, {});
  }
}
