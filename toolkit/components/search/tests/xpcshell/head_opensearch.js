/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// eslint-disable-next-line no-undef
XPCOMUtils.defineLazyModuleGetters(this, {
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * Fake the installation of an add-on in the profile, by creating the
 * directory and registering it with the directory service.
 *
 * @param {string} [name]
 *   The engine name to install.
 */
function installAddonEngine(name = "engine-addon") {
  const profD = do_get_profile().QueryInterface(Ci.nsIFile);

  let dir = profD.clone();
  dir.append("extensions");
  if (!dir.exists()) {
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }

  dir.append("search-engine@tests.mozilla.org");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/install.rdf").copyTo(dir, "install.rdf");
  dir.append("searchplugins");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_get_file("data/" + name + ".xml").copyTo(dir, "bug645970.xml");
}

/**
 * Copy the engine-distribution.xml engine to a fake distribution
 * created in the profile, and registered with the directory service.
 */
function installDistributionEngine() {
  const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

  const profD = do_get_profile().QueryInterface(Ci.nsIFile);

  let dir = profD.clone();
  dir.append("distribution");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  let distDir = dir.clone();

  dir.append("searchplugins");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  dir.append("common");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine-override.xml").copyTo(dir, "bug645970.xml");

  Services.dirsvc.registerProvider({
    getFile(aProp, aPersistent) {
      aPersistent.value = true;
      if (aProp == XRE_APP_DISTRIBUTION_DIR) {
        return distDir.clone();
      }
      return null;
    },
  });
}
