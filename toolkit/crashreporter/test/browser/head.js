function create_subdir(dir, subdirname) {
  let subdir = dir.clone();
  subdir.append(subdirname);
  if (subdir.exists()) {
    subdir.remove(true);
  }
  subdir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return subdir;
}

// need to hold on to this to unregister for cleanup
var _provider = null;

function make_fake_appdir() {
  // Create a directory inside the profile and register it as UAppData, so
  // we can stick fake crash reports inside there. We put it inside the profile
  // just because we know that will get cleaned up after the mochitest run.
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"]
               .getService(Ci.nsIProperties);
  let profD = dirSvc.get("ProfD", Ci.nsILocalFile);
  // create a subdir just to keep our files out of the way
  let appD = create_subdir(profD, "UAppData");

  let crashesDir = create_subdir(appD, "Crash Reports");
  create_subdir(crashesDir, "pending");
  create_subdir(crashesDir, "submitted");

  _provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "UAppData") {
        return appD.clone();
      }
      // Depending on timing we can get requests for other files.
      // When we threw an exception here, in the world before bug 997440, this got lost
      // because of the arbitrary JSContext being used in XPCWrappedJSClass::CallMethod.
      // After bug 997440 this gets reported to our window and causes the tests to fail.
      // So, we'll just dump out a message to the logs.
      dump("WARNING: make_fake_appdir - fake nsIDirectoryServiceProvider - Unexpected getFile for: '" + prop + "'\n");
      return null;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  // register our new provider
  dirSvc.QueryInterface(Ci.nsIDirectoryService)
        .registerProvider(_provider);
  // and undefine the old value
  try {
    dirSvc.undefine("UAppData");
  } catch (ex) {} // it's ok if this fails, the value might not be cached yet
  return appD.clone();
}

function cleanup_fake_appdir() {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"]
               .getService(Ci.nsIProperties);
  dirSvc.QueryInterface(Ci.nsIDirectoryService)
        .unregisterProvider(_provider);
  // undefine our value so future calls get the real value
  try {
    dirSvc.undefine("UAppData");
  } catch (ex) {
    dump("cleanup_fake_appdir: dirSvc.undefine failed: " + ex.message + "\n");
  }
}

function add_fake_crashes(crD, count) {
  let results = [];
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);
  let submitdir = crD.clone();
  submitdir.append("submitted");
  // create them from oldest to newest, to ensure that about:crashes
  // displays them in the correct order
  let date = Date.now() - count * 60000;
  for (let i = 0; i < count; i++) {
    let uuid = uuidGenerator.generateUUID().toString();
    // ditch the {}
    uuid = "bp-" + uuid.substring(1, uuid.length - 2);
    let fn = uuid + ".txt";
    let file = submitdir.clone();
    file.append(fn);
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
    file.lastModifiedTime = date;
    results.push({'id': uuid, 'date': date, 'pending': false});

    date += 60000;
  }
  // we want them sorted newest to oldest, since that's the order
  // that about:crashes lists them in
  results.sort((a, b) => b.date - a.date);
  return results;
}

function writeDataToFile(file, data) {
  var fstream = Cc["@mozilla.org/network/file-output-stream;1"]
                .createInstance(Ci.nsIFileOutputStream);
  // open, write, truncate
  fstream.init(file, -1, -1, 0);
  var os = Cc["@mozilla.org/intl/converter-output-stream;1"]
           .createInstance(Ci.nsIConverterOutputStream);
  os.init(fstream, "UTF-8", 0, 0x0000);
  os.writeString(data);
  os.close();
  fstream.close();
}

function addPendingCrashreport(crD, date, extra) {
  let pendingdir = crD.clone();
  pendingdir.append("pending");
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);
  let uuid = uuidGenerator.generateUUID().toString();
  // ditch the {}
  uuid = uuid.substring(1, uuid.length - 1);
  let dumpfile = pendingdir.clone();
  dumpfile.append(uuid + ".dmp");
  writeDataToFile(dumpfile, "MDMP"); // that's the start of a valid minidump, anyway
  let extrafile = pendingdir.clone();
  extrafile.append(uuid + ".extra");
  let extradata = "";
  for (let x in extra) {
    extradata += x + "=" + extra[x] + "\n";
  }
  writeDataToFile(extrafile, extradata);
  let memoryfile = pendingdir.clone();
  memoryfile.append(uuid + ".memory.json.gz");
  writeDataToFile(memoryfile, "Let's pretend this is a memory report");
  dumpfile.lastModifiedTime = date;
  extrafile.lastModifiedTime = date;
  memoryfile.lastModifiedTime = date;
  return {'id': uuid, 'date': date, 'pending': true, 'extra': extra};
}
