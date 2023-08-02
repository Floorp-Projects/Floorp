/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const modules = ["utils.sys.mjs", "WeaveCrypto.sys.mjs"];

function run_test() {
  for (let m of modules) {
    let resource = "resource://services-crypto/" + m;
    _("Attempting to import: " + resource);
    ChromeUtils.importESModule(resource);
  }
}
