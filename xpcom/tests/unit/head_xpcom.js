var isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);

function get_test_program(prog)
{
  var progPath = do_get_cwd();
  progPath.append(prog);
  if (isWindows)
    progPath.leafName = progPath.leafName + ".exe";
  return progPath;
}

function set_process_running_environment()
{
  var envSvc = Components.classes["@mozilla.org/process/environment;1"].
    getService(Components.interfaces.nsIEnvironment);
  var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"].
    getService(Components.interfaces.nsIProperties);
  var greDir = dirSvc.get("GreD", Components.interfaces.nsIFile);
  var macOSDir = greDir.parent;
  macOSDir.append("MacOS");
  envSvc.set("DYLD_LIBRARY_PATH", macOSDir.path);
  // For Linux
  envSvc.set("LD_LIBRARY_PATH", greDir.path);
  //XXX: handle windows
}

