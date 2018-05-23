/* This code is mostly copied from chrome/test/unit/head_crtestutils.js */

const NS_CHROME_MANIFESTS_FILE_LIST = "ChromeML";
const XUL_CACHE_PREF = "nglayout.debug.disable_xul_cache";

ChromeUtils.import("resource://gre/modules/Services.jsm");

var gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                    getService(Ci.nsIXULChromeRegistry);

// Create the temporary file in the profile, instead of in TmpD, because
// we know the mochitest harness kills off the profile when it's done.
function copyToTemporaryFile(f) {
  let tmpd = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let tmpf = tmpd.clone();
  tmpf.append("temp.manifest");
  tmpf.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
  tmpf.remove(false);
  f.copyTo(tmpd, tmpf.leafName);
  return tmpf;
}

function* dirIter(directory) {
  var testsDir = Services.io.newURI(directory)
                  .QueryInterface(Ci.nsIFileURL).file;

  let en = testsDir.directoryEntries;
  while (en.hasMoreElements()) {
    yield en.nextFile;
  }
}

function getParent(path) {
  let lastSlash = path.lastIndexOf("/");
  if (lastSlash == -1) {
    lastSlash = path.lastIndexOf("\\");
    if (lastSlash == -1) {
      return "";
    }
    return "/" + path.substring(0, lastSlash).replace(/\\/g, "/");
  }
  return path.substring(0, lastSlash);
}

function copyDirToTempProfile(path, subdirname) {

  if (subdirname === undefined) {
    subdirname = "mochikit-tmp";
  }

  let tmpdir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  tmpdir.append(subdirname);
  tmpdir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o777);

  let rootDir = getParent(path);
  if (rootDir == "") {
    return tmpdir;
  }

  // The SimpleTest directory is hidden
  var files = Array.from(dirIter("file://" + rootDir));
  for (let f in files) {
    files[f].copyTo(tmpdir, "");
  }
  return tmpdir;

}

function convertChromeURI(chromeURI) {
  let uri = Services.io.newURI(chromeURI);
  return gChromeReg.convertChromeURL(uri);
}

function chromeURIToFile(chromeURI) {
  var jar = getJar(chromeURI);
  if (jar) {
    var tmpDir = extractJarToTmp(jar);
    let parts = chromeURI.split("/");
    if (parts[parts.length - 1] != "") {
      tmpDir.append(parts[parts.length - 1]);
    }
    return tmpDir;
  }

  return convertChromeURI(chromeURI).
    QueryInterface(Ci.nsIFileURL).file;
}

// Register a chrome manifest temporarily and return a function which un-does
// the registrarion when no longer needed.
function createManifestTemporarily(tempDir, manifestText) {
  Services.prefs.setBoolPref(XUL_CACHE_PREF, true);

  tempDir.append("temp.manifest");

  let foStream = Cc["@mozilla.org/network/file-output-stream;1"]
                   .createInstance(Ci.nsIFileOutputStream);
  foStream.init(tempDir,
                0x02 | 0x08 | 0x20, 0o664, 0); // write, create, truncate
  foStream.write(manifestText, manifestText.length);
  foStream.close();
  let tempfile = copyToTemporaryFile(tempDir);

  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
    autoRegister(tempfile);

  gChromeReg.refreshSkins();

  return function() {
    tempfile.fileSize = 0; // truncate the manifest
    gChromeReg.checkForNewChrome();
    gChromeReg.refreshSkins();
    Services.prefs.clearUserPref(XUL_CACHE_PREF);
  };
}

// Register a chrome manifest temporarily and return a function which un-does
// the registrarion when no longer needed.
function registerManifestTemporarily(manifestURI) {
  Services.prefs.setBoolPref(XUL_CACHE_PREF, true);

  let file = chromeURIToFile(manifestURI);

  let tempfile = copyToTemporaryFile(file);
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
    autoRegister(tempfile);

  gChromeReg.refreshSkins();

  return function() {
    tempfile.fileSize = 0; // truncate the manifest
    gChromeReg.checkForNewChrome();
    gChromeReg.refreshSkins();
    Services.prefs.clearUserPref(XUL_CACHE_PREF);
  };
}

function registerManifestPermanently(manifestURI) {
  var chromepath = chromeURIToFile(manifestURI);

  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
    autoRegister(chromepath);
  return chromepath;
}
