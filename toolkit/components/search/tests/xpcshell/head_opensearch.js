/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// eslint-disable-next-line no-undef
XPCOMUtils.defineLazyModuleGetters(this, {
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * Copy the engine-distribution.xml engine to a fake distribution
 * created in the profile, and registered with the directory service.
 *
 * @param {string} [sourcePath]
 *   The file to use for the engine source.
 * @param {string} [targetName]
 *   The name of the file to use for the engine in the distribution directory.
 * @returns {nsIFile}
 *   An object referencing the distribution directory.
 */
function installDistributionEngine(
  sourcePath = "data/engine-override.xml",
  targetName = "basic.xml"
) {
  const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

  // Use a temp directory rather than the profile or app directory, as then the
  // engine gets registered as a proper [distribution] load path rather than
  // something else.
  let dir = do_get_tempdir();
  dir.append("distribution");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  let distDir = dir.clone();

  dir.append("searchplugins");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  dir.append("common");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file(sourcePath).copyTo(dir, targetName);

  Services.dirsvc.registerProvider({
    getFile(aProp, aPersistent) {
      aPersistent.value = true;
      if (aProp == XRE_APP_DISTRIBUTION_DIR) {
        return distDir.clone();
      }
      return null;
    },
  });
  return distDir;
}
