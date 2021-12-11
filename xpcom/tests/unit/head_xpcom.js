let CC = Components.Constructor;

function get_test_program(prog) {
  var progPath = do_get_cwd();
  progPath.append(prog);
  progPath.leafName = progPath.leafName + mozinfo.bin_suffix;
  return progPath;
}

function set_process_running_environment() {
  var envSvc = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  // Importing Services here messes up appInfo for some of the tests.
  // eslint-disable-next-line mozilla/use-services
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(
    Ci.nsIProperties
  );
  var greBinDir = dirSvc.get("GreBinD", Ci.nsIFile);
  envSvc.set("DYLD_LIBRARY_PATH", greBinDir.path);
  // For Linux
  envSvc.set("LD_LIBRARY_PATH", greBinDir.path);
  // XXX: handle windows
}
